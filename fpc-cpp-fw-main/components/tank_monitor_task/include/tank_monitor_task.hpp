/**
 * @file tank_monitor_task.hpp
 * @brief FreeRTOS task wrapper around one or more ITankMonitor instances.
 *
 * Calls `ITankMonitor::check_level()` on every managed monitor every
 * `check_interval_ms` milliseconds in a single dedicated FreeRTOS task.
 *
 * Construction and wiring (creating Tank, LevelSensor, TankMonitor objects,
 * subscribing event callbacks, etc.) happens outside this component.  This
 * class only drives the polling loop and manages the task lifecycle.
 *
 * @code
 *   TankMonitor tm1{cfg1};  tm1.init();
 *   TankMonitor tm2{cfg2};  tm2.init();
 *
 *   TankMonitorTaskConfig cfg;
 *   cfg.id                = 1;
 *   cfg.monitors          = {&tm1, &tm2};   // non-owning pointers
 *   cfg.check_interval_ms = 500;
 *
 *   TankMonitorTask task{cfg};
 *   task.start();
 *   // ...
 *   task.stop();
 * @endcode
 */

#pragma once

#include <cstdint>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common.hpp"
#include "tank_monitor.hpp"   // brings in ITankMonitor

namespace fpc {

// ─── Config ───────────────────────────────────────────────────────────────────

struct TankMonitorTaskConfig {
    int32_t                    id{0};
    std::vector<ITankMonitor*> monitors;              ///< Non-owning; must outlive the task.
    uint32_t                   check_interval_ms{1000};
    uint32_t                   stack_size{4096};
    UBaseType_t                priority{5};
};

// ─── TankMonitorTask ──────────────────────────────────────────────────────────

class TankMonitorTask {
public:
    explicit TankMonitorTask(TankMonitorTaskConfig config);

    /// Destructor stops the task if it is still running.
    ~TankMonitorTask();

    TankMonitorTask(const TankMonitorTask&)            = delete;
    TankMonitorTask& operator=(const TankMonitorTask&) = delete;

    /// Spawns a FreeRTOS task that calls check_level() on every monitor
    /// every check_interval_ms milliseconds.
    /// Returns InvalidParameter if monitors list is empty,
    ///         InvalidState if already running.
    [[nodiscard]] Result<void> start();

    /// Signals the task to stop and blocks until it has exited.
    /// Returns InvalidState if not running.
    [[nodiscard]] Result<void> stop();

    [[nodiscard]] bool is_running() const noexcept;

private:
    static void task_fn(void* param) noexcept;

    TankMonitorTaskConfig config_;
    TaskHandle_t          handle_{nullptr};
    volatile bool         running_{false};
};

}  // namespace fpc
