/**
 * @file webserver_task.hpp
 * @brief FreeRTOS task wrapper that owns a Webserver lifecycle.
 *
 * An optional WebserverSetupFn callback is invoked after the server starts
 * to let callers register HTTP routes (replaces hardcoded setup_web_handlers).
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <cstdint>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common.hpp"
#include "webserver.hpp"

namespace fpc {

using WebserverSetupFn = std::function<Result<void>(IWebServer&)>;

struct WebserverTaskConfig {
    WebserverConfig  webserver_config{};
    WebserverSetupFn setup_fn{};
    uint32_t         stack_size{8192};
    UBaseType_t      priority{5};
};

class WebserverTask final {
public:
    explicit WebserverTask(WebserverTaskConfig config);
    ~WebserverTask();

    WebserverTask(const WebserverTask&)            = delete;
    WebserverTask& operator=(const WebserverTask&) = delete;

    [[nodiscard]] Result<void> start();
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool is_running() const noexcept;

private:
    static void task_fn(void* param) noexcept;

    WebserverTaskConfig config_;
    TaskHandle_t        handle_{nullptr};
    volatile bool       running_{false};
};

} // namespace fpc
