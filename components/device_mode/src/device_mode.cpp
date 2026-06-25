#include "device_mode.hpp"
#include "esp_system.h"
#include "esp_log.h"

static const char* TAG = "DEVICE_MODE";

namespace fpc {

DeviceMode::DeviceMode(DeviceModeConfig config) noexcept
    : config_{std::move(config)} {}

DeviceMode::~DeviceMode() {
    if (initialized_) {
        gpio_isr_handler_remove(config_.button_pin);
    }
}

IRAM_ATTR void DeviceMode::isr_handler(void* /*arg*/) noexcept {
    esp_restart();
}

Result<void> DeviceMode::init() {
    if (config_.button_pin == GPIO_NUM_NC) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    gpio_config_t io_cfg{};
    io_cfg.intr_type    = GPIO_INTR_ANYEDGE;
    io_cfg.mode         = GPIO_MODE_INPUT;
    io_cfg.pin_bit_mask = (1ULL << config_.button_pin);
    io_cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;

    if (gpio_config(&io_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed");
        return Result<void>::err(SystemError::Failed);
    }

    if (gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed");
        return Result<void>::err(SystemError::Failed);
    }

    if (gpio_isr_handler_add(config_.button_pin, isr_handler, nullptr) != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed");
        return Result<void>::err(SystemError::Failed);
    }

    initialized_ = true;
    return Result<void>::ok();
}

Result<void> DeviceMode::handle_event() {
    int level = gpio_get_level(config_.button_pin);

    if (level == 0) {
        ESP_LOGI(TAG, "Button pressed — starting webserver (config mode)");
        if (!config_.webserver_cb) {
            return Result<void>::err(SystemError::NullParameter);
        }
        config_.webserver_cb();
    } else {
        ESP_LOGI(TAG, "Button released — starting main tasks (normal mode)");
        if (!config_.main_task_cb) {
            return Result<void>::err(SystemError::NullParameter);
        }
        config_.main_task_cb();
    }

    return Result<void>::ok();
}

bool DeviceMode::is_initialized() const noexcept {
    return initialized_;
}

} // namespace fpc
