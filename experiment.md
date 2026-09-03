# Current Sensor / Pump Monitor Profiling Experiment — Task Prompt

**Task:** Design and implement a hardware-in-the-loop profiling experiment for the Current Sensor + Pump Monitor + Relay subsystem, using the existing `components` libraries (`current_sensor.hpp`, `pump_monitor.hpp`, `relay_driver.hpp`, `pump_control_unit.hpp`, `pump_monitor_task.hpp`). No new abstractions should be introduced unless a metric cannot be measured with existing interfaces — in that case, propose the smallest possible extension (e.g., a timestamped `ReadCallback` wrapper) and call it out explicitly in the design doc.

## Deliverable 1 — Experiment Design Document

Place in `components/current_sensor/doc/` or a new `docs/experiments/` folder.

For **each** metric below, specify: hypothesis/target value, exact measurement method (which API calls, what is timestamped, with what clock — `esp_timer_get_time()` vs FreeRTOS ticks), required sample size / duration, pass/fail acceptance criteria, and known confounds (I2C bus speed for ADS1115, ADC oversampling, task scheduling jitter, relay coil switching time, debounce).

1. **Current-read latency** — single-call latency of `ICurrentSensor::read()` for both `InternalAdcAcs712Config` and `Ads1115Acs712Config` backends. Report min/mean/max/p99 over N ≥ 1000 calls.
2. **Current-read accuracy** — comparison against a reference (bench multimeter / calibrated shunt) across a current sweep (e.g., 0A, 0.5A, 1A...rated). Report error in Amps and % of full scale, plus zero-offset drift over time/temperature.
3. **Consecutive-read throughput** — max sustainable sampling rate (Hz) for back-to-back `read()` calls, and how it changes with `number_of_samples` in `PumpMonitorConfig` (i.e., cost of `check_current()` as a whole, not just one `read()`).
4. **Overcurrent detection tolerance/margin** — using `current_analytics_basic_decision` / `current_analytics_capacity_decision`, determine the smallest current excursion above `current_rating` (from `PumpConfig`) that reliably triggers `PumpStateMachineState::Overcurrent`, and the false-positive/false-negative rate near threshold.
5. **State-change reaction latency** — end-to-end time from an injected overcurrent event (test current source step change) to the subscriber callback firing (`PumpMonitorEventCallback` via `subscribe()`), measured via timestamp injected at the moment of the physical/injected step vs. timestamp captured inside the callback.
6. **Relay trip latency** — time from `PumpMonitor` detecting overcurrent to `IRelay::trip()` completing (GPIO line change), measured at the GPIO pin with a scope/logic analyzer if available, otherwise via `esp_timer_get_time()` bracketing the `trip()` call chain wired through a subscriber in `main.cpp`.
7. **Multi-pump scaling behavior** — with `PumpControlUnit` managing the maximum supported number of `PumpMonitor` instances (`kMaxMonitors` = 10) each polled via its own `PumpMonitorTask` (or a shared loop via `loop_pump_monitors()`), measure: per-cycle loop duration, jitter/drift in `check_interval_ms`, CPU/task starvation effects, and whether reaction latency (metric 5/6) degrades as pump count increases from 1 → max.

Document must also specify: test rig wiring (ADC/ADS1115 channel mapping, relay GPIO, injected load or programmable current source), FreeRTOS task priorities/stack sizes used, sdkconfig options relevant to ADC/I2C timing, and how raw data will be logged (UART/log level, or SPIFFS CSV) for offline analysis.

## Deliverable 2 — Experiment Firmware

- New `main.cpp` (or a separate experiment entry point selectable via Kconfig/build flag, not disruptive to the existing production `main.cpp` flow) that wires up `CurrentSensor`, `PumpMonitor`, `Relay`, `PumpControlUnit`, and `PumpMonitorTask`(s) purely from component APIs — no new hardware drivers.
- Instrumentation: high-resolution timestamps (`esp_timer_get_time()`) around `read()`, `check_current()`, and relay `trip()`/`on()`/`off()` calls; results logged via `ESP_LOGI`/`ESP_LOGW` in a parseable format (e.g., CSV lines) so they can be captured from the serial monitor.
- A configurable mode (compile-time constant or Kconfig) to select which of the 6 experiments runs, plus a "max pumps" mode instantiating up to `PumpControlUnit::kMaxMonitors` pump monitors with synthetic/injected current profiles for repeatable overcurrent triggering (via injectable `ReadCallback` stub for cases without a real programmable source).
- Must build cleanly under the existing ESP-IDF CMake project (respect `CMakeLists.txt` and component `CMakeLists.txt` patterns) and not break the existing production build target.

## Non-goals / constraints

- Do not modify existing component public interfaces unless strictly necessary; if changed, update all call sites and existing tests under each component's `test/`.
- Do not remove or alter the current production `main.cpp` boot flow (DeviceMode → webserver_task / main_tasks) — the experiment must be reachable without breaking normal operation (e.g., separate build target, Kconfig switch, or clearly marked `#ifdef`).
- All new/changed code must have accompanying unit tests using the existing test scaffolding conventions (see any `components/*/test/CMakeLists.txt`).
