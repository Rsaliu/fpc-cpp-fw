/**
 * @file event_handler_task.hpp
 * @brief FreeRTOS task that dispatches relay actions from a monitor event queue.
 *
 * Receives TankEvent messages from an injected QueueHandle_t and calls
 * IRelay::on() / IRelay::off() based on the event type.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "common.hpp"
#include "relay_driver.hpp"
#include "event.hpp"

namespace fpc {

struct TankEvent {
    EventType event{EventType::TankNormal};
};

struct EventHandlerTaskConfig {
    IRelay*       relay{nullptr};
    QueueHandle_t event_queue{nullptr};
    uint32_t      queue_wait_ms{1000};
    uint32_t      stack_size{4096};
    UBaseType_t   priority{5};
};

class EventHandlerTask final {
public:
    explicit EventHandlerTask(EventHandlerTaskConfig config);
    ~EventHandlerTask();

    EventHandlerTask(const EventHandlerTask&)            = delete;
    EventHandlerTask& operator=(const EventHandlerTask&) = delete;

    [[nodiscard]] Result<void> start();
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool is_running() const noexcept;

private:
    static void task_fn(void* param) noexcept;

    EventHandlerTaskConfig config_;
    TaskHandle_t           handle_{nullptr};
    volatile bool          running_{false};
};

} // namespace fpc
