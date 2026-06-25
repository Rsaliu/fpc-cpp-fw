#include "pump_monitor.hpp"
#include "esp_log.h"

static const char* TAG = "pump_monitor";

namespace fpc {

// ─── Constructor ──────────────────────────────────────────────────────────────

PumpMonitor::PumpMonitor(PumpMonitorConfig config)
    : config_(std::move(config))
{
    for (auto& sub : subscribers_) {
        sub.id     = -1;
        sub.in_use = false;
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Result<void> PumpMonitor::init()
{
    if (!config_.read_cb || !config_.analytics_cb) {
        ESP_LOGE(TAG, "Null callback in PumpMonitorConfig");
        return Result<void>::err(SystemError::NullParameter);
    }
    if (config_.number_of_samples <= 0 || config_.number_of_samples > kMaxSamples) {
        ESP_LOGE(TAG, "Invalid number_of_samples: %d", (int)config_.number_of_samples);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (state_ != PumpMonitorState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    state_ = PumpMonitorState::Initialized;
    ESP_LOGI(TAG, "PumpMonitor [%d] initialized", (int)config_.id);
    return Result<void>::ok();
}

Result<void> PumpMonitor::deinit()
{
    if (state_ == PumpMonitorState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    state_ = PumpMonitorState::NotInitialized;
    ESP_LOGI(TAG, "PumpMonitor [%d] deinitialized", (int)config_.id);
    return Result<void>::ok();
}

// ─── Core logic ───────────────────────────────────────────────────────────────

Result<void> PumpMonitor::check_current()
{
    if (state_ != PumpMonitorState::Initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }

    for (int32_t i = 0; i < config_.number_of_samples; ++i) {
        auto res = config_.read_cb();
        if (res.is_err()) {
            ESP_LOGE(TAG, "read_cb failed on sample %d", (int)i);
            return Result<void>::err(res.error());
        }
        samples_[i] = res.value();
    }

    const float rated = config_.pump_config.current_rating;
    const float min_w = config_.pump_config.min_working_current;
    const auto  prev  = sm_state_;

    sm_state_ = config_.analytics_cb(
        Span<const float>{samples_, static_cast<size_t>(config_.number_of_samples)},
        rated,
        min_w);

    if (sm_state_ != prev) {
        ESP_LOGW(TAG, "State changed: %d -> %d",
                 static_cast<int>(prev), static_cast<int>(sm_state_));
        notify_subscribers(sm_state_to_event(sm_state_));
    }

    return Result<void>::ok();
}

// ─── Accessors ────────────────────────────────────────────────────────────────

PumpMonitorState PumpMonitor::state() const noexcept
{
    return state_;
}

int32_t PumpMonitor::id() const noexcept
{
    return config_.id;
}

const PumpMonitorConfig& PumpMonitor::config() const noexcept
{
    return config_;
}

// ─── Subscriber management ────────────────────────────────────────────────────

Result<int32_t> PumpMonitor::subscribe(PumpMonitorEventCallback callback)
{
    if (!callback) {
        return Result<int32_t>::err(SystemError::NullParameter);
    }
    if (state_ != PumpMonitorState::Initialized) {
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

Result<void> PumpMonitor::unsubscribe(int32_t event_id)
{
    if (event_id < 0 || event_id >= kMaxSubscribers) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (state_ != PumpMonitorState::Initialized) {
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

EventType PumpMonitor::sm_state_to_event(PumpStateMachineState s) noexcept
{
    switch (s) {
        case PumpStateMachineState::Normal:       return EventType::PumpNormal;
        case PumpStateMachineState::Undercurrent: return EventType::PumpUndercurrent;
        case PumpStateMachineState::Overcurrent:  return EventType::PumpOvercurrent;
        default:                                  return EventType::Unknown;
    }
}

void PumpMonitor::notify_subscribers(EventType event) noexcept
{
    for (int32_t i = 0; i < kMaxSubscribers; ++i) {
        if (subscribers_[i].in_use && subscribers_[i].callback) {
            subscribers_[i].callback(event, subscribers_[i].id);
        }
    }
}

}  // namespace fpc
