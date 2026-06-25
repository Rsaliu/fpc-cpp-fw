#include "webserver_task.hpp"
#include "esp_log.h"

static const char* TAG = "webserver_task";

namespace fpc {

WebserverTask::WebserverTask(WebserverTaskConfig config)
    : config_{std::move(config)} {}

WebserverTask::~WebserverTask()
{
    if (running_) (void)stop();
}

Result<void> WebserverTask::start()
{
    if (running_) return Result<void>::err(SystemError::InvalidState);
    running_ = true;
    BaseType_t ret = xTaskCreate(task_fn, "webserver_task",
                                 config_.stack_size, this,
                                 config_.priority, &handle_);
    if (ret != pdPASS) {
        running_ = false; handle_ = nullptr;
        return Result<void>::err(SystemError::OperationFailed);
    }
    return Result<void>::ok();
}

Result<void> WebserverTask::stop()
{
    if (!running_) return Result<void>::err(SystemError::InvalidState);
    running_ = false;
    for (int i = 0; i < 30 && handle_ != nullptr; ++i)
        vTaskDelay(pdMS_TO_TICKS(100));
    if (handle_ != nullptr) { vTaskDelete(handle_); handle_ = nullptr; }
    return Result<void>::ok();
}

bool WebserverTask::is_running() const noexcept { return running_; }

void WebserverTask::task_fn(void* param) noexcept
{
    WebserverTask* self = static_cast<WebserverTask*>(param);
    Webserver server{self->config_.webserver_config};

    if (server.init().is_err() || server.start().is_err()) {
        ESP_LOGE(TAG, "Webserver init/start failed");
        (void)server.deinit();
        self->running_ = false; self->handle_ = nullptr;
        vTaskDelete(nullptr); return;
    }

    if (self->config_.setup_fn) {
        if (self->config_.setup_fn(server).is_err()) {
            ESP_LOGE(TAG, "setup_fn failed");
            (void)server.stop(); (void)server.deinit();
            self->running_ = false; self->handle_ = nullptr;
            vTaskDelete(nullptr); return;
        }
    }

    ESP_LOGI(TAG, "WebserverTask running");
    while (self->running_) vTaskDelay(pdMS_TO_TICKS(1000));

    (void)server.stop(); (void)server.deinit();
    self->handle_ = nullptr;
    vTaskDelete(nullptr);
}

} // namespace fpc
