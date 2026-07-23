# pump_monitor_task

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `pump_monitor`

---

## What it does

`pump_monitor_task` wraps a `IPumpMonitor` in a **FreeRTOS task** that calls `check_current()` on a fixed interval. This decouples the "how often to poll" concern (here) from the "what to do with the reading" concern (in `pump_monitor`).

The task does not own the monitor — it holds a non-owning pointer. Construction, wiring, and lifetime of the monitor happen outside this component, in `Factory` and `main.cpp`.

---

## Public API

### `PumpMonitorTaskConfig`

```cpp
struct PumpMonitorTaskConfig {
    int32_t       id{0};
    IPumpMonitor* monitor{nullptr};         // non-owning; must outlive the task
    uint32_t      check_interval_ms{1000};  // default: poll every 1 second
    uint32_t      stack_size{4096};         // FreeRTOS task stack (bytes)
    UBaseType_t   priority{5};
};
```

---

### `PumpMonitorTask`

```cpp
class PumpMonitorTask {
public:
    explicit PumpMonitorTask(PumpMonitorTaskConfig config);
    ~PumpMonitorTask();  // stops the task if still running (RAII)

    PumpMonitorTask(const PumpMonitorTask&)            = delete;
    PumpMonitorTask& operator=(const PumpMonitorTask&) = delete;

    // Spawn a FreeRTOS task that calls check_current() every check_interval_ms.
    [[nodiscard]] Result<void> start();

    // Signal the task to stop and wait for it to exit.
    [[nodiscard]] Result<void> stop();

    [[nodiscard]] bool is_running() const noexcept;
};
```

---

## Task loop (simplified)

```cpp
while (running_) {
    monitor->check_current();
    vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
}
```

---

## Usage (from `main.cpp`)

```cpp
PumpMonitorTaskConfig cfg;
cfg.id                = 1;
cfg.monitor           = app.pump_monitors[0].get();
cfg.check_interval_ms = 100;  // poll every 100 ms

auto task = std::make_unique<PumpMonitorTask>(cfg);
task->start();
// task stored in g_runtime.pump_tasks to keep it alive
```

In `main.cpp` the `check_interval_ms` is set to **100 ms** — pump current is sampled 10 times per second.

---

## Lifecycle

```
PumpMonitorTask::start()  → xTaskCreate(task_fn, ...)
PumpMonitorTask::stop()   → sets running_ = false, waits for task to finish
~PumpMonitorTask()        → calls stop() if task is still running
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `xTaskCreate(pump_poll_task, ...)` scattered in `main.c` | `PumpMonitorTask::start()` encapsulates creation |
| `void* params` cast to `pump_monitor_t*` inside task | `PumpMonitorTaskConfig` captured by value — no cast |
| No cleanup on error paths | Destructor stops task automatically (RAII) |
