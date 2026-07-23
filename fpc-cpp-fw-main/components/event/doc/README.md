# event

**Type:** Header-only library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`event` defines the **system-wide event vocabulary** — the set of named things that can happen and the data that travels alongside them. It is used by monitors (`pump_monitor`, `tank_monitor`) to publish state changes and by subscribers (relays, tasks) to react to them.

This component is header-only; there is no compiled code.

---

## Public API

### `EventType` — strongly-typed event codes

```cpp
enum class EventType : uint8_t {
    TankNormal       = 0, // Tank level is within the normal range
    TankFull         = 1, // Tank level reached the full threshold
    TankLow          = 2, // Tank level dropped below the low threshold
    PumpOvercurrent  = 3, // Pump current exceeded the rated maximum
    PumpNormal       = 4, // Pump current is within the normal range
    PumpUndercurrent = 5, // Pump current fell below minimum working level
    Unknown          = 6, // Unrecognised / uninitialised event
};

constexpr std::string_view to_string(EventType e) noexcept;
```

---

### `EventPayload` — typed data alongside the event

```cpp
// Possible payload variants:
struct FloatPayload    { float value; };
struct Uint16Payload   { uint16_t value; };
struct EmptyPayload    {};

using EventPayload = std::variant<EmptyPayload, FloatPayload, Uint16Payload>;
```

`std::variant` eliminates the `void*` context pointer pattern from the original C code. You cannot accidentally read the wrong type from a variant.

---

### `MonitorEvent` — the message placed on FreeRTOS queues

```cpp
struct MonitorEvent {
    EventType    type{EventType::Unknown};
    int32_t      monitor_id{-1};     // which monitor published this
    int32_t      subscriber_id{-1};  // which subscriber slot to call
    EventPayload payload{EmptyPayload{}};
};
```

`MonitorEvent` is the struct that gets `xQueueSend()`-ed from a monitor to an event handler task.

---

## Event flow

```
PumpMonitor detects overcurrent
        │
        ▼
PumpMonitorEventCallback(EventType::PumpOvercurrent, subscriber_id)
        │
        ▼
EventHandlerTask pops MonitorEvent from queue
        │
        ▼
IRelay::on() / off()  depending on event type
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `event_type_t` plain C enum (global namespace) | `enum class EventType : uint8_t` (scoped) |
| `void*` payload alongside event callbacks | `std::variant<EmptyPayload, FloatPayload, Uint16Payload>` — type-safe |
| Manual event-type-to-string switch in multiple files | Single `constexpr to_string(EventType)` in this header |
