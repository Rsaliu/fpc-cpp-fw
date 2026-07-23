/**
 * @file setup_config_button.hpp
 * @brief GPIO button handler: selects normal vs webserver boot mode.
 *
 * Identical in purpose to device_mode.hpp but exposed as a separate
 * component to match the reference project's setup_config_button component.
 * IRAM_ATTR is only on the .cpp definition — never in this header.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <functional>
#include "driver/gpio.h"
#include "common.hpp"

namespace fpc {

using ButtonCallback = std::function<void()>;

struct SetupConfigButtonConfig {
    gpio_num_t     button_pin{GPIO_NUM_NC};
    ButtonCallback main_task_cb{};
    ButtonCallback webserver_cb{};
};

class SetupConfigButton final {
public:
    explicit SetupConfigButton(SetupConfigButtonConfig config) noexcept;
    ~SetupConfigButton();

    SetupConfigButton(const SetupConfigButton&)            = delete;
    SetupConfigButton& operator=(const SetupConfigButton&) = delete;

    [[nodiscard]] Result<void> init();
    [[nodiscard]] Result<void> handle_event();
    [[nodiscard]] bool is_initialized() const noexcept;

private:
    // IRAM_ATTR only in .cpp definition.
    static void isr_handler(void* arg) noexcept;

    SetupConfigButtonConfig config_;
    bool                    initialized_{false};
};

} // namespace fpc
