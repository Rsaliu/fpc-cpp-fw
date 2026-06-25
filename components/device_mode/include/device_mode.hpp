/**
 * @file device_mode.hpp
 * @brief Boot-time mode selection via a GPIO button.
 *
 * Reads a single GPIO pin at startup:
 *   - Button pressed  (level == 0) → invoke `webserver_cb` (config mode).
 *   - Button released (level == 1) → invoke `main_task_cb` (normal mode).
 *
 * An ANYEDGE ISR is also installed; it calls `esp_restart()` so the user can
 * force a mode switch by pressing the button at any time (matches reference
 * behaviour exactly).
 *
 * C++17 improvements over the reference:
 *   - `std::function<void()>` callbacks replace raw function pointers.
 *   - `Result<void>` return values replace raw `error_type_t`.
 *   - RAII: ISR is removed in the destructor if init() succeeded.
 */

#pragma once

#include <functional>
#include "driver/gpio.h"
#include "common.hpp"

namespace fpc {

using DeviceCallback = std::function<void()>;

struct DeviceModeConfig {
    gpio_num_t     button_pin{GPIO_NUM_NC};  ///< GPIO pin wired to the mode button.
    DeviceCallback main_task_cb{};           ///< Called when button is not pressed.
    DeviceCallback webserver_cb{};           ///< Called when button is pressed.
};

class DeviceMode final {
public:
    explicit DeviceMode(DeviceModeConfig config) noexcept;
    ~DeviceMode();

    DeviceMode(const DeviceMode&)            = delete;
    DeviceMode& operator=(const DeviceMode&) = delete;

    /**
     * @brief Configure GPIO input with pull-up and install ANYEDGE ISR.
     *
     * Fails with:
     *   - SystemError::InvalidParameter  if button_pin is GPIO_NUM_NC.
     *   - SystemError::Failed            if gpio_config() or ISR install fails.
     */
    [[nodiscard]] Result<void> init();

    /**
     * @brief Read button level and invoke the appropriate callback.
     *
     * Level 0 (pressed)  → webserver_cb().
     * Level 1 (released) → main_task_cb().
     *
     * Fails with SystemError::NullParameter if the required callback is null.
     */
    [[nodiscard]] Result<void> handle_event();

    [[nodiscard]] bool is_initialized() const noexcept;

private:
    static void isr_handler(void* arg) noexcept;

    DeviceModeConfig config_;
    bool             initialized_{false};
};

} // namespace fpc
