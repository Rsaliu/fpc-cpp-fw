#include "pump_control_unit.hpp"
#include "esp_log.h"

static const char* TAG = "pump_control_unit";

namespace fpc {

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Result<void> PumpControlUnit::init()
{
    if (initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    initialized_ = true;
    ESP_LOGI(TAG, "PumpControlUnit initialized");
    return Result<void>::ok();
}

Result<void> PumpControlUnit::deinit()
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    initialized_ = false;
    ESP_LOGI(TAG, "PumpControlUnit deinitialized");
    return Result<void>::ok();
}

bool PumpControlUnit::is_initialized() const noexcept
{
    return initialized_;
}

// ─── Pump-monitor registry ────────────────────────────────────────────────────

Result<void> PumpControlUnit::add_pump_monitor(IPumpMonitor& monitor)
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (static_cast<int32_t>(pump_monitors_.size()) >= kMaxMonitors) {
        return Result<void>::err(SystemError::BufferOverflow);
    }
    const int32_t mid = monitor.id();
    if (pump_monitors_.count(mid)) {
        ESP_LOGW(TAG, "PumpMonitor id=%d already registered", (int)mid);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    pump_monitors_.emplace(mid,monitor);
    ESP_LOGI(TAG, "PumpMonitor id=%d added", (int)mid);
    return Result<void>::ok();
}

Result<void> PumpControlUnit::remove_pump_monitor(int32_t id)
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    auto it = pump_monitors_.find(id);
    if (it == pump_monitors_.end()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    pump_monitors_.erase(it);
    ESP_LOGI(TAG, "PumpMonitor id=%d removed", (int)id);
    return Result<void>::ok();
}

// ─── Tank-monitor registry ────────────────────────────────────────────────────

Result<void> PumpControlUnit::add_tank_monitor(ITankMonitor& monitor)
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (static_cast<int32_t>(tank_monitors_.size()) >= kMaxMonitors) {
        return Result<void>::err(SystemError::BufferOverflow);
    }
    const int32_t mid = monitor.id();
    if (tank_monitors_.count(mid)) {
        ESP_LOGW(TAG, "TankMonitor id=%d already registered", (int)mid);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    tank_monitors_.emplace(mid, monitor);
    ESP_LOGI(TAG, "TankMonitor id=%d added", (int)mid);
    return Result<void>::ok();
}

Result<void> PumpControlUnit::remove_tank_monitor(int32_t id)
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    auto it = tank_monitors_.find(id);
    if (it == tank_monitors_.end()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    tank_monitors_.erase(it);
    ESP_LOGI(TAG, "TankMonitor id=%d removed", (int)id);
    return Result<void>::ok();
}

// ─── Per-monitor subscriber helpers ──────────────────────────────────────────

Result<int32_t> PumpControlUnit::add_subscriber_to_pump_monitor(
    int32_t                  pm_id,
    PumpMonitorEventCallback callback)
{
    if (!initialized_) {
        return Result<int32_t>::err(SystemError::InvalidState);
    }
    auto it = pump_monitors_.find(pm_id);
    if (it == pump_monitors_.end()) {
        ESP_LOGW(TAG, "PumpMonitor id=%d not found for subscribe", (int)pm_id);
        return Result<int32_t>::err(SystemError::InvalidParameter);
    }
    auto res = it->second.get().subscribe(std::move(callback));
    if (res.is_ok()) {
        ESP_LOGI(TAG, "Subscribed to PumpMonitor id=%d, event_id=%d",
                 (int)pm_id, (int)res.value());
    }
    return res;
}

Result<void> PumpControlUnit::remove_subscriber_from_pump_monitor(
    int32_t pm_id,
    int32_t event_id)
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    auto it = pump_monitors_.find(pm_id);
    if (it == pump_monitors_.end()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    auto res = it->second.get().unsubscribe(event_id);
    if (res.is_ok()) {
        ESP_LOGI(TAG, "Unsubscribed from PumpMonitor id=%d, event_id=%d",
                 (int)pm_id, (int)event_id);
    }
    return res;
}


Result<int32_t> PumpControlUnit::add_subscriber_to_tank_monitor(
    int32_t                  tm_id,
    TankMonitorEventCallback callback)
{
    if (!initialized_) {
        return Result<int32_t>::err(SystemError::InvalidState);
    }
    auto it = tank_monitors_.find(tm_id);
    if (it == tank_monitors_.end()) {
        ESP_LOGW(TAG, "TankMonitor id=%d not found for subscribe", (int)tm_id);
        return Result<int32_t>::err(SystemError::InvalidParameter);
    }
    auto res = it->second.get().subscribe(std::move(callback));
    if (res.is_ok()) {
        ESP_LOGI(TAG, "Subscribed to PumpMonitor id=%d, event_id=%d",
                 (int)tm_id, (int)res.value());
    }
    return res;
}

Result<void> PumpControlUnit::remove_subscriber_from_tank_monitor(
    int32_t tm_id,
    int32_t event_id)
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    auto it = tank_monitors_.find(tm_id);
    if (it == tank_monitors_.end()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    auto res = it->second.get().unsubscribe(event_id);
    if (res.is_ok()) {
        ESP_LOGI(TAG, "Unsubscribed from TankMonitor id=%d, event_id=%d",
                 (int)tm_id, (int)event_id);
    }
    return res;
}


// ─── Polling loops ────────────────────────────────────────────────────────────

Result<void> PumpControlUnit::loop_pump_monitors()
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    for (auto& [id, pm] : pump_monitors_) {
        auto res = pm.get().check_current();
        if (res.is_err()) {
            ESP_LOGE(TAG, "check_current failed on PumpMonitor id=%d: %d",
                     (int)id, (int)res.error());
        }
    }
    return Result<void>::ok();
}

Result<void> PumpControlUnit::loop_level_monitors()
{
    if (!initialized_) {
        return Result<void>::err(SystemError::InvalidState);
    }
    for (auto& [id, tm] : tank_monitors_) {
        auto res = tm.get().check_level();
        if (res.is_err()) {
            ESP_LOGE(TAG, "check_level failed on TankMonitor id=%d: %d",
                     (int)id, (int)res.error());
        }
    }
    return Result<void>::ok();
}

}  // namespace fpc
