#include "tank_monitor_task.hpp"
#include <cstdio>
#include "esp_log.h"

static const char* TAG = "tank_monitor_task";

namespace fpc {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

TankMonitorTask::TankMonitorTask(TankMonitorTaskConfig config)
    : config_(std::move(config))
{}

TankMonitorTask::~TankMonitorTask()
{
    if (running_) {
        stop();
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Result<void> TankMonitorTask::start()
{
    if (config_.monitors.empty()) {
        ESP_LOGE(TAG, "monitors list is empty — cannot start task %d", (int)config_.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (running_) {
        ESP_LOGW(TAG, "TankMonitorTask [%d] already running", (int)config_.id);
        return Result<void>::err(SystemError::InvalidState);
    }

    running_ = true;

    char task_name[32];
    std::snprintf(task_name, sizeof(task_name), "tm_task_%d", (int)config_.id);

    BaseType_t ret = xTaskCreate(
        task_fn, task_name,
        config_.stack_size, this,
        config_.priority, &handle_);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed for TankMonitorTask [%d]", (int)config_.id);
        running_ = false;
        handle_  = nullptr;
        return Result<void>::err(SystemError::OperationFailed);
    }

    ESP_LOGI(TAG, "TankMonitorTask [%d] started (%zu monitors)",
             (int)config_.id, config_.monitors.size());
    return Result<void>::ok();
}

Result<void> TankMonitorTask::stop()
{
    if (!running_) {
        return Result<void>::err(SystemError::InvalidState);
    }

    running_ = false;

    // Give the task up to 3 s to exit cleanly.
    for (int i = 0; i < 30 && handle_ != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (handle_ != nullptr) {
        vTaskDelete(handle_);
        handle_ = nullptr;
    }

    ESP_LOGI(TAG, "TankMonitorTask [%d] stopped", (int)config_.id);
    return Result<void>::ok();
}

bool TankMonitorTask::is_running() const noexcept
{
    return running_;
}

// ─── Task function ────────────────────────────────────────────────────────────

void TankMonitorTask::task_fn(void* param) noexcept
{
    TankMonitorTask* self = static_cast<TankMonitorTask*>(param);
    ESP_LOGI(TAG, "TankMonitorTask [%d] loop started", (int)self->config_.id);

    while (self->running_) {
        for (ITankMonitor* mon : self->config_.monitors) {
            if (mon == nullptr) {
                ESP_LOGW(TAG, "nullptr monitor in TankMonitorTask [%d] — skipping",
                         (int)self->config_.id);
                continue;
            }
            auto res = mon->check_level();
            if (res.is_err()) {
                ESP_LOGE(TAG, "check_level error: %d", (int)res.error());
            }
        }
        vTaskDelay(pdMS_TO_TICKS(self->config_.check_interval_ms));
    }

    ESP_LOGI(TAG, "TankMonitorTask [%d] loop exited", (int)self->config_.id);
    self->handle_ = nullptr;
    vTaskDelete(nullptr);
}

}  // namespace fpc
