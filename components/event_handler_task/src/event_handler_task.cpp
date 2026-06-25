#include "event_handler_task.hpp"
#include "esp_log.h"

static const char* TAG = "event_handler_task";

namespace fpc {

EventHandlerTask::EventHandlerTask(EventHandlerTaskConfig config)
    : config_{std::move(config)} {}

EventHandlerTask::~EventHandlerTask()
{
    if (running_) (void)stop();
}

Result<void> EventHandlerTask::start()
{
    if (config_.relay == nullptr)
        return Result<void>::err(SystemError::NullParameter);
    if (config_.event_queue == nullptr)
        return Result<void>::err(SystemError::NullParameter);
    if (running_)
        return Result<void>::err(SystemError::InvalidState);

    running_ = true;
    BaseType_t ret = xTaskCreate(task_fn, "evt_handler_task",
                                 config_.stack_size, this,
                                 config_.priority, &handle_);
    if (ret != pdPASS) {
        running_ = false; handle_ = nullptr;
        return Result<void>::err(SystemError::OperationFailed);
    }
    ESP_LOGI(TAG, "EventHandlerTask started");
    return Result<void>::ok();
}

Result<void> EventHandlerTask::stop()
{
    if (!running_) return Result<void>::err(SystemError::InvalidState);
    running_ = false;
    for (int i = 0; i < 30 && handle_ != nullptr; ++i)
        vTaskDelay(pdMS_TO_TICKS(100));
    if (handle_ != nullptr) { vTaskDelete(handle_); handle_ = nullptr; }
    return Result<void>::ok();
}

bool EventHandlerTask::is_running() const noexcept { return running_; }

void EventHandlerTask::task_fn(void* param) noexcept
{
    EventHandlerTask* self = static_cast<EventHandlerTask*>(param);
    while (self->running_) {
        TankEvent ev{};
        if (xQueueReceive(self->config_.event_queue, &ev,
                          pdMS_TO_TICKS(self->config_.queue_wait_ms)) != pdPASS)
            continue;

        switch (ev.event) {
        case EventType::TankFull:
            if (auto r = self->config_.relay->off(); r.is_err())
                ESP_LOGE(TAG, "relay->off() failed");
            break;
        case EventType::TankLow:
            if (auto r = self->config_.relay->on(); r.is_err())
                ESP_LOGE(TAG, "relay->on() failed");
            break;
        case EventType::TankNormal:
            ESP_LOGI(TAG, "Tank normal");
            break;
        default:
            break;
        }
    }
    self->handle_ = nullptr;
    vTaskDelete(nullptr);
}

} // namespace fpc
