# subscriber_event_task

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `event`, `tank_monitor`

---

## What it does

`subscriber_event_task` is a **FreeRTOS task** that subscribes to a `ITankMonitor`'s event stream and processes the events it receives. It registers a `TankMonitorEventCallback` with the monitor during `start()` and unregisters it during `stop()`.

While running, the task logs its subscription state at a configurable interval, making it useful for both live operation and debugging.

---

## Public API

### `SubscriberEventTaskConfig`

```cpp
struct SubscriberEventTaskConfig {
    ITankMonitor*            monitor{nullptr};       // non-owning; must outlive the task
    TankMonitorEventCallback event_callback{};       // called when the monitor fires an event
    uint32_t                 log_interval_ms{1000};  // how often to log subscription state
    uint32_t                 stack_size{4096};
    UBaseType_t              priority{5};
};
```

---

### `SubscriberEventTask`

```cpp
class SubscriberEventTask final {
public:
    explicit SubscriberEventTask(SubscriberEventTaskConfig config);
    ~SubscriberEventTask();  // calls stop() if running (RAII)

    Result<void> start();   // registers callback with monitor, spawns task
    Result<void> stop();    // unregisters callback, signals task to exit
    bool         is_running() const noexcept;
};
```

---

## Lifecycle

```
start()
  │
  ├── monitor->add_subscriber(event_callback)  ← registers, stores slot id
  │
  └── xTaskCreate(task_fn, ...)
           │
     while (running_) {
         vTaskDelay(log_interval_ms)
         ESP_LOGI("subscriber state: ...")
     }

stop()
  │
  ├── running_ = false          ← signals task loop to exit
  └── monitor->remove_subscriber(subscription_id_)
```

---

## Relationship to `EventHandlerTask`

Both components subscribe to monitor events and react to them. The key difference:

| Component | Reacts to | Drives |
|---|---|---|
| `EventHandlerTask` | `TankEvent` from a FreeRTOS queue | `IRelay` (on/off) |
| `SubscriberEventTask` | `TankMonitorEventCallback` from monitor directly | Logging / custom `event_callback` |

`SubscriberEventTask` is more flexible — the action taken on each event is fully injected via `event_callback`, so it can be used for logging, telemetry, or any other reaction.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Global subscriber slot managed manually | `subscription_id_` stored in the class; auto-unregistered on `stop()` |
| `void*` event callback context | `TankMonitorEventCallback` with lambda capture |
| Manual `vTaskDelete()` | Destructor calls `stop()` (RAII) |
