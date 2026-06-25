# pump_monitor

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `event`, `pump`, `current_sensor`

---

## What it does

`pump_monitor` watches a pump by periodically sampling its current draw and running it through an analytics function. When the current state changes (Normal → Overcurrent, Normal → Undercurrent, etc.), it fires event callbacks to all registered subscribers.

It defines a pure interface (`IPumpMonitor`) and a concrete implementation (`PumpMonitor`) with injectable analytics so the decision logic can be replaced or tested independently of hardware.

---

## State machine

```
         ┌──────────────────────────────┐
         ▼                              │
       Normal ──overcurrent──► Overcurrent
         │                              │
         │ ◄────── normal ──────────────┘
         │
         ├──undercurrent──► Undercurrent
         │                              │
         └──────── normal ──────────────┘
```

```cpp
enum class PumpStateMachineState : uint8_t {
    Normal       = 0,
    Undercurrent = 1,
    Overcurrent  = 2,
};
```

---

## Key types

### `AnalyticsCallback` — injected decision function

```cpp
using AnalyticsCallback = std::function<
    PumpStateMachineState(Span<const float>, float rated_current, float min_working_current)
>;
```

The default implementation is `current_analytics_basic_decision()` which averages the sample window and compares against thresholds.

### `PumpMonitorEventCallback` — subscriber callbacks

```cpp
using PumpMonitorEventCallback = std::function<void(EventType, int32_t)>;
```

Called whenever the state machine transitions, with the new event type and the subscriber's slot ID.

---

## Public API (interface)

```cpp
class IPumpMonitor {
public:
    virtual Result<void> init()          = 0;
    virtual Result<void> deinit()        = 0;
    virtual Result<void> check_current() = 0;  // called by PumpMonitorTask

    virtual Result<int32_t> add_subscriber(PumpMonitorEventCallback cb)    = 0;
    virtual Result<void>    remove_subscriber(int32_t id)                  = 0;

    virtual int32_t id() const noexcept = 0;
};
```

### `PumpMonitorConfig`

```cpp
struct PumpMonitorConfig {
    int32_t          id{0};
    PumpConfig       pump_config{};        // provides current_rating and min_working_current
    ReadCallback     read_cb{};            // std::function<Result<float>()>
    int32_t          number_of_samples{1}; // sliding window size
    AnalyticsCallback analytics_cb{};      // defaults to current_analytics_basic_decision
};
```

---

## How `check_current()` works

1. Calls `read_cb()` to sample the current sensor.
2. Adds the sample to a circular buffer (up to `number_of_samples`).
3. Calls `analytics_cb(samples, rated_current, min_working_current)` to decide the new state.
4. If the state has changed, fires `PumpMonitorEventCallback` on every registered subscriber.

---

## Subscribers

Up to `kMaxSubscribers` callbacks can be registered. Each `add_subscriber()` returns a slot ID that can be used with `remove_subscriber()`.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `pump_monitor_t` global struct | Non-copyable `PumpMonitor` class |
| `void*` analytics context pointer | `AnalyticsCallback` with lambda capture |
| Fixed-size C array for samples | `Span<const float>` — non-owning view |
| `event_type_t` enum (global) | `EventType` scoped enum class |
| Manual subscriber array iteration | `std::array<PumpMonitorSubscriber, kMaxSubscribers>` |
