/**
 * @file pump_monitor_task.hpp
 * @brief FreeRTOS task wrapper around IPumpMonitor.
 *
 * Calls `IPumpMonitor::check_current()` every `check_interval_ms` milliseconds
 * in a dedicated FreeRTOS task.  Construction and wiring of the concrete
 * monitor (PumpMonitor, mock, etc.) happens outside this component — this class
 * only manages the task lifecycle.
 *
 * @code
 *   PumpMonitor monitor{...};
 *   monitor.init();
 *
 *   PumpMonitorTaskConfig cfg;
 *   cfg.id = 1;
 *   cfg.monitor = &monitor;
 *   cfg.check_interval_ms = 500;
 *
 *   PumpMonitorTask task{cfg};
 *   task.start();   // spawns FreeRTOS task
 *   // ...
 *   task.stop();    // signals task to exit, waits for it
 * @endcode
 */

#pragma once

#include <cstdint>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common.hpp"
#include "pump_monitor.hpp"   // brings in IPumpMonitor

namespace fpc {

// ─── Config ───────────────────────────────────────────────────────────────────

struct PumpMonitorTaskConfig {
    int32_t       id{0};
    IPumpMonitor* monitor{nullptr};          ///< Non-owning; must outlive the task.
    uint32_t      check_interval_ms{1000};   ///< Period between check_current() calls.
    uint32_t      stack_size{4096};          ///< FreeRTOS task stack in bytes.
    UBaseType_t   priority{5};               ///< FreeRTOS task priority.
};

// ─── PumpMonitorTask ──────────────────────────────────────────────────────────

class PumpMonitorTask {
public:
    explicit PumpMonitorTask(PumpMonitorTaskConfig config);

    /// Destructor stops the task if it is still running.
    ~PumpMonitorTask();

    PumpMonitorTask(const PumpMonitorTask&)            = delete;
    PumpMonitorTask& operator=(const PumpMonitorTask&) = delete;

    /// Spawns a FreeRTOS task that calls check_current() every
    /// check_interval_ms milliseconds.
    /// Returns InvalidState if already running, NullParameter if monitor==nullptr.
    [[nodiscard]] Result<void> start();

    /// Signals the task to stop and blocks until it has exited.
    /// Returns InvalidState if not running.
    [[nodiscard]] Result<void> stop();

    [[nodiscard]] bool is_running() const noexcept;

private:
    static void task_fn(void* param) noexcept;

    PumpMonitorTaskConfig config_;
    TaskHandle_t          handle_{nullptr};
    volatile bool         running_{false};
};

}  // namespace fpc
