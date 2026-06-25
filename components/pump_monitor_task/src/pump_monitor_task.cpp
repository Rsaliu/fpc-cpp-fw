#include "pump_monitor_task.hpp"
#include <cstdio>
#include "esp_log.h"

static const char* TAG = "pump_monitor_task";

namespace fpc {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

PumpMonitorTask::PumpMonitorTask(PumpMonitorTaskConfig config)
    : config_(std::move(config))
{}

PumpMonitorTask::~PumpMonitorTask()
{
    if (running_) {
        stop();
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Result<void> PumpMonitorTask::start()
{
    if (config_.monitor == nullptr) {
        ESP_LOGE(TAG, "monitor is nullptr — cannot start task %d", (int)config_.id);
        return Result<void>::err(SystemError::NullParameter);
    }
    if (running_) {
        ESP_LOGW(TAG, "PumpMonitorTask [%d] already running", (int)config_.id);
        return Result<void>::err(SystemError::InvalidState);
    }

    running_ = true;

    char task_name[32];
    std::snprintf(task_name, sizeof(task_name), "pm_task_%d", (int)config_.id);

    BaseType_t ret = xTaskCreate(
        task_fn, task_name,
        config_.stack_size, this,
        config_.priority, &handle_);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate failed for PumpMonitorTask [%d]", (int)config_.id);
        running_ = false;
        handle_  = nullptr;
        return Result<void>::err(SystemError::OperationFailed);
    }

    ESP_LOGI(TAG, "PumpMonitorTask [%d] started", (int)config_.id);
    return Result<void>::ok();
}

Result<void> PumpMonitorTask::stop()
{
    if (!running_) {
        return Result<void>::err(SystemError::InvalidState);
    }

    running_ = false;

    // Give the task up to 3 s to exit cleanly by clearing handle_ itself.
    for (int i = 0; i < 30 && handle_ != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (handle_ != nullptr) {
        vTaskDelete(handle_);
        handle_ = nullptr;
    }

    ESP_LOGI(TAG, "PumpMonitorTask [%d] stopped", (int)config_.id);
    return Result<void>::ok();
}

bool PumpMonitorTask::is_running() const noexcept
{
    return running_;
}

// ─── Task function ────────────────────────────────────────────────────────────

void PumpMonitorTask::task_fn(void* param) noexcept
{
    PumpMonitorTask* self = static_cast<PumpMonitorTask*>(param);
    ESP_LOGI(TAG, "PumpMonitorTask [%d] loop started", (int)self->config_.id);

    while (self->running_) {
        auto res = self->config_.monitor->check_current();
        if (res.is_err()) {
            ESP_LOGE(TAG, "check_current error: %d", (int)res.error());
        }
        vTaskDelay(pdMS_TO_TICKS(self->config_.check_interval_ms));
    }

    ESP_LOGI(TAG, "PumpMonitorTask [%d] loop exited", (int)self->config_.id);
    self->handle_ = nullptr;
    vTaskDelete(nullptr);
}

}  // namespace fpc
