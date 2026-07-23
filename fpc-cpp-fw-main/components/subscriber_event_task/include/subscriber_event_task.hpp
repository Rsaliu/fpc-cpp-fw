/**
 * @file subscriber_event_task.hpp
 * @brief FreeRTOS task that subscribes to an ITankMonitor event stream.
 *
 * Subscribes during start(), logs subscription state at log_interval_ms cadence,
 * and unsubscribes during stop().
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <cstdint>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common.hpp"
#include "event.hpp"
#include "tank_monitor.hpp"

namespace fpc {

struct SubscriberEventTaskConfig {
    ITankMonitor*            monitor{nullptr};
    TankMonitorEventCallback event_callback{};
    uint32_t                 log_interval_ms{1000};
    uint32_t                 stack_size{4096};
    UBaseType_t              priority{5};
};

class SubscriberEventTask final {
public:
    explicit SubscriberEventTask(SubscriberEventTaskConfig config);
    ~SubscriberEventTask();

    SubscriberEventTask(const SubscriberEventTask&)            = delete;
    SubscriberEventTask& operator=(const SubscriberEventTask&) = delete;

    [[nodiscard]] Result<void> start();
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool is_running() const noexcept;

private:
    static void task_fn(void* param) noexcept;

    SubscriberEventTaskConfig config_;
    TaskHandle_t              handle_{nullptr};
    volatile bool             running_{false};
    int32_t                   subscription_id_{-1};
};

} // namespace fpc
