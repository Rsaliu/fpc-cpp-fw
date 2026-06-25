# event_handler_task

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `relay_driver`, `event`

---

## What it does

`event_handler_task` is a **FreeRTOS task** that watches a queue for `TankEvent` messages and drives a relay in response. When a tank event arrives, it calls `IRelay::on()` or `IRelay::off()` depending on the event type.

This decouples the monitor (which only fires events) from the relay driver (which only responds to commands) — the task bridges them asynchronously via a FreeRTOS queue.

---

## Public API

### `TankEvent`

```cpp
struct TankEvent {
    EventType event{EventType::TankNormal};
};
```

The message struct placed on the FreeRTOS queue by a tank monitor or subscriber.

---

### `EventHandlerTaskConfig`

```cpp
struct EventHandlerTaskConfig {
    IRelay*       relay{nullptr};       // relay to actuate (non-owning)
    QueueHandle_t event_queue{nullptr}; // source of TankEvent messages
    uint32_t      queue_wait_ms{1000};  // how long to block waiting for a message
    uint32_t      stack_size{4096};
    UBaseType_t   priority{5};
};
```

---

### `EventHandlerTask`

```cpp
class EventHandlerTask final {
public:
    explicit EventHandlerTask(EventHandlerTaskConfig config);
    ~EventHandlerTask();  // stops task if running

    Result<void> start();
    Result<void> stop();
    bool         is_running() const noexcept;
};
```

---

## Task behaviour

Inside the FreeRTOS task loop:

```
xQueueReceive(event_queue, &tank_event, queue_wait_ms)
        │
   event.type == TankFull  →  relay->off()   (tank full: stop filling)
   event.type == TankLow   →  relay->on()    (tank low: start filling)
   event.type == TankNormal →  (no action)
```

---

## Lifecycle

```
EventHandlerTask::start()   → xTaskCreate(task_fn, ...)
EventHandlerTask::stop()    → sets running_ = false, waits for task to exit
~EventHandlerTask()         → calls stop() if still running (RAII)
```

---

## Example

```cpp
QueueHandle_t q = xQueueCreate(10, sizeof(TankEvent));

EventHandlerTaskConfig cfg{
    .relay       = &my_relay,
    .event_queue = q,
};
EventHandlerTask handler{cfg};
handler.start();

// From another task or ISR:
TankEvent ev{EventType::TankLow};
xQueueSend(q, &ev, 0);
// → handler task calls relay->on()
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Bare `xTaskCreate(task_fn, ...)` call site in application code | `EventHandlerTask::start()` encapsulates task creation |
| `void*` passed to task for context | `EventHandlerTaskConfig` captured by value inside the class |
| Manual `vTaskDelete()` at shutdown | Destructor calls `stop()` automatically (RAII) |
