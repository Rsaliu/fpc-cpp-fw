/**
 * @file experiment.hpp
 * @brief Entry point for the current-sensor / pump-monitor / relay profiling
 *        experiments described in docs/experiments/current_sensor_pump_monitor_profiling.md
 *
 * Activated only when CONFIG_FPC_EXPERIMENT_MODE_ENABLE=y (see main/Kconfig.projbuild).
 * Does not alter the production app_main() boot flow when disabled.
 */

#pragma once

namespace fpc::experiment {

/// Dispatches to the experiment selected by CONFIG_FPC_EXPERIMENT_ID.
/// Blocks forever (log-and-loop) once the experiment completes, so it is
/// safe to call directly from app_main().
void run();

} // namespace fpc::experiment
