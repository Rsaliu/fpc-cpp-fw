#include "tank_monitor.hpp"
#include <cstdio>
#include "esp_log.h"
#include <freertos/FreeRTOS.h>

static const char* TAG = "tank_monitor";
constexpr int kDefaultReadDelayMs = 500;  // Delay between samples in milliseconds

namespace fpc {

// ─── Constructor ──────────────────────────────────────────────────────────────

TankMonitor::TankMonitor(TankMonitorConfig config)
    : config_(std::move(config))
{
    for (auto& sub : subscribers_) {
        sub.id     = -1;
        sub.in_use = false;
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Result<void> TankMonitor::init()
{
    if (!config_.level_read_cb || !config_.analytics_cb) {
        ESP_LOGE(TAG, "Null callback in TankMonitorConfig");
        return Result<void>::err(SystemError::NullParameter);
    }
    if (config_.number_of_samples <= 0 || config_.number_of_samples > kMaxSamples) {
        ESP_LOGE(TAG, "Invalid number_of_samples: %d", (int)config_.number_of_samples);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (state_ != TankMonitorState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    state_ = TankMonitorState::Initialized;
    ESP_LOGI(TAG, "TankMonitor [%d] initialized", (int)config_.id);
    return Result<void>::ok();
}

Result<void> TankMonitor::deinit()
{
    if (state_ == TankMonitorState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    state_ = TankMonitorState::NotInitialized;
    ESP_LOGI(TAG, "TankMonitor [%d] deinitialized", (int)config_.id);
    return Result<void>::ok();
}

// ─── Core logic ───────────────────────────────────────────────────────────────

Result<void> TankMonitor::check_level()
{
    if (state_ != TankMonitorState::Initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }

    for (int32_t i = 0; i < config_.number_of_samples; ++i) {
        auto res = config_.level_read_cb();
        if (res.is_err()) {
            ESP_LOGE(TAG, "level_read_cb failed on sample %d", (int)i);
            return Result<void>::err(res.error());
        }
        samples_[i] = res.value();
        vTaskDelay(pdMS_TO_TICKS(kDefaultReadDelayMs));  // Small delay between samples to avoid hammering the sensor
    }

    const int32_t full_mm = config_.tank_config.full_level_mm;
    const int32_t low_mm  = config_.tank_config.low_level_mm;
    const auto    prev    = sm_state_;

    sm_state_ = config_.analytics_cb(
        Span<const uint16_t>{samples_, static_cast<size_t>(config_.number_of_samples)},
        full_mm,
        low_mm);

    if (sm_state_ != prev) {
        ESP_LOGW(TAG, "State changed: %d -> %d",
                 static_cast<int>(prev), static_cast<int>(sm_state_));
        notify_subscribers(sm_state_to_event(sm_state_));
    }

    return Result<void>::ok();
}

// ─── Accessors ────────────────────────────────────────────────────────────────

TankMonitorState TankMonitor::state() const noexcept
{
    return state_;
}

int32_t TankMonitor::id() const noexcept
{
    return config_.id;
}

const TankMonitorConfig& TankMonitor::config() const noexcept
{
    return config_;
}

TankStateMachineState TankMonitor::sm_state() const noexcept
{
    return sm_state_;
}

Result<void> TankMonitor::format_info_into(MutableByteView buf) const noexcept
{
    if (buf.empty()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    int written = std::snprintf(
        reinterpret_cast<char*>(buf.data()), buf.size(),
        "TankMonitor ID=%d state=%d sm_state=%d subscribers=%d",
        (int)config_.id,
        (int)state_,
        (int)sm_state_,
        (int)subscriber_count_);
    if (written < 0) {
        return Result<void>::err(SystemError::OperationFailed);
    }
    if (static_cast<size_t>(written) >= buf.size()) {
        return Result<void>::err(SystemError::BufferOverflow);
    }
    return Result<void>::ok();
}

// ─── Subscriber management ────────────────────────────────────────────────────

Result<int32_t> TankMonitor::subscribe(TankMonitorEventCallback callback)
{
    if (!callback) {
        return Result<int32_t>::err(SystemError::NullParameter);
    }
    if (state_ != TankMonitorState::Initialized) {
        return Result<int32_t>::err(SystemError::InvalidState);
    }
    if (subscriber_count_ >= kMaxSubscribers) {
        return Result<int32_t>::err(SystemError::BufferOverflow);
    }

    for (int32_t i = 0; i < kMaxSubscribers; ++i) {
        if (!subscribers_[i].in_use) {
            subscribers_[i].id       = i;
            subscribers_[i].callback = std::move(callback);
            subscribers_[i].in_use   = true;
            ++subscriber_count_;
            ESP_LOGI(TAG, "Subscriber registered at slot %d", (int)i);
            return Result<int32_t>::ok(i);
        }
    }
    return Result<int32_t>::err(SystemError::BufferOverflow);
}

Result<void> TankMonitor::unsubscribe(int32_t event_id)
{
    if (event_id < 0 || event_id >= kMaxSubscribers) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (state_ != TankMonitorState::Initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (!subscribers_[event_id].in_use) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    subscribers_[event_id].in_use   = false;
    subscribers_[event_id].id       = -1;
    subscribers_[event_id].callback = nullptr;
    --subscriber_count_;
    ESP_LOGI(TAG, "Subscriber unregistered from slot %d", (int)event_id);
    return Result<void>::ok();
}

// ─── Private helpers ──────────────────────────────────────────────────────────

EventType TankMonitor::sm_state_to_event(TankStateMachineState s) noexcept
{
    switch (s) {
        case TankStateMachineState::Normal: return EventType::TankNormal;
        case TankStateMachineState::Full:   return EventType::TankFull;
        case TankStateMachineState::Low:    return EventType::TankLow;
        default:                            return EventType::Unknown;
    }
}

void TankMonitor::notify_subscribers(EventType event) noexcept
{
    for (int32_t i = 0; i < kMaxSubscribers; ++i) {
        if (subscribers_[i].in_use && subscribers_[i].callback) {
            subscribers_[i].callback(event, subscribers_[i].id);
        }
    }
}

}  // namespace fpc
