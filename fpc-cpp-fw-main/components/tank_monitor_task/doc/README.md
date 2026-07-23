# tank_monitor_task

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `tank_monitor`

---

## What it does

`tank_monitor_task` wraps **one or more `ITankMonitor` instances** in a single **FreeRTOS task** that calls `check_level()` on each monitor every `check_interval_ms` milliseconds.

Unlike `pump_monitor_task` (which wraps a single monitor), one `TankMonitorTask` can poll multiple tank monitors in a single task — reducing FreeRTOS task overhead when there are several tanks.

---

## Public API

### `TankMonitorTaskConfig`

```cpp
struct TankMonitorTaskConfig {
    int32_t                    id{0};
    std::vector<ITankMonitor*> monitors;              // non-owning; must outlive the task
    uint32_t                   check_interval_ms{1000};
    uint32_t                   stack_size{4096};
    UBaseType_t                priority{5};
};
```

---

### `TankMonitorTask`

```cpp
class TankMonitorTask {
public:
    explicit TankMonitorTask(TankMonitorTaskConfig config);
    ~TankMonitorTask();  // stops the task if still running (RAII)

    TankMonitorTask(const TankMonitorTask&)            = delete;
    TankMonitorTask& operator=(const TankMonitorTask&) = delete;

    [[nodiscard]] Result<void> start();
    [[nodiscard]] Result<void> stop();
    [[nodiscard]] bool         is_running() const noexcept;
};
```

---

## Task loop (simplified)

```cpp
while (running_) {
    for (auto* monitor : monitors_) {
        monitor->check_level();
    }
    vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
}
```

---

## Usage (from `main.cpp`)

```cpp
TankMonitorTaskConfig cfg;
cfg.id                = 1;
cfg.check_interval_ms = 1000;  // poll tanks every 1 second
cfg.monitors.reserve(app.tank_monitors.size());

for (auto& tm : app.tank_monitors) {
    cfg.monitors.push_back(tm.get());
}

auto task = std::make_unique<TankMonitorTask>(std::move(cfg));
task->start();
```

Tank monitors are polled at **1 000 ms (1 second)** intervals by default.

---

## Multiple monitors, one task

```
TankMonitorTask (1 FreeRTOS task)
    │  every check_interval_ms:
    ├── tank_monitor_1.check_level()
    ├── tank_monitor_2.check_level()
    └── tank_monitor_N.check_level()
```

This is more efficient than spawning N separate tasks when all tanks share the same polling period.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| One `xTaskCreate()` per tank monitor | One `TankMonitorTask` per polling group; `std::vector` of monitors |
| `void*` cast to get monitor pointer | `TankMonitorTaskConfig` with `std::vector<ITankMonitor*>` |
| Manual `vTaskDelete()` | Destructor calls `stop()` (RAII) |
