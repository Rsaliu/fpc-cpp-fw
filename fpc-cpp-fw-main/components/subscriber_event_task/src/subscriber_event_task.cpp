#include "subscriber_event_task.hpp"
#include "esp_log.h"

static const char* TAG = "subscriber_event_task";

namespace fpc {

SubscriberEventTask::SubscriberEventTask(SubscriberEventTaskConfig config)
    : config_{std::move(config)} {}

SubscriberEventTask::~SubscriberEventTask()
{
    if (running_) (void)stop();
}

Result<void> SubscriberEventTask::start()
{
    if (config_.monitor == nullptr)
        return Result<void>::err(SystemError::NullParameter);
    if (!config_.event_callback)
        return Result<void>::err(SystemError::NullParameter);
    if (running_)
        return Result<void>::err(SystemError::InvalidState);

    auto sub_result = config_.monitor->subscribe(config_.event_callback);
    if (sub_result.is_err()) return Result<void>::err(sub_result.error());
    subscription_id_ = sub_result.value();
    ESP_LOGI(TAG, "Subscribed id=%d", (int)subscription_id_);

    running_ = true;
    BaseType_t ret = xTaskCreate(task_fn, "sub_evt_task",
                                 config_.stack_size, this,
                                 config_.priority, &handle_);
    if (ret != pdPASS) {
        running_ = false; handle_ = nullptr;
        (void)config_.monitor->unsubscribe(subscription_id_);
        subscription_id_ = -1;
        return Result<void>::err(SystemError::OperationFailed);
    }
    return Result<void>::ok();
}

Result<void> SubscriberEventTask::stop()
{
    if (!running_) return Result<void>::err(SystemError::InvalidState);
    running_ = false;
    for (int i = 0; i < 30 && handle_ != nullptr; ++i)
        vTaskDelay(pdMS_TO_TICKS(100));
    if (handle_ != nullptr) { vTaskDelete(handle_); handle_ = nullptr; }

    if (config_.monitor != nullptr && subscription_id_ >= 0) {
        (void)config_.monitor->unsubscribe(subscription_id_);
        subscription_id_ = -1;
    }
    return Result<void>::ok();
}

bool SubscriberEventTask::is_running() const noexcept { return running_; }

void SubscriberEventTask::task_fn(void* param) noexcept
{
    SubscriberEventTask* self = static_cast<SubscriberEventTask*>(param);
    while (self->running_) {
        ESP_LOGI(TAG, "Waiting for events (sub_id=%d)", (int)self->subscription_id_);
        vTaskDelay(pdMS_TO_TICKS(self->config_.log_interval_ms));
    }
    self->handle_ = nullptr;
    vTaskDelete(nullptr);
}

} // namespace fpc
