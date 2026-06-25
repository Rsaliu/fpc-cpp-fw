# tank_monitor

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `event`, `tank`

---

## What it does

`tank_monitor` watches a tank by periodically reading its level sensor and running the result through an analytics function. When the water level transitions between states (Normal ↔ Full ↔ Low), it fires event callbacks to all registered subscribers.

It defines a pure interface (`ITankMonitor`) and a concrete implementation (`TankMonitor`) with injectable analytics so the decision logic can be replaced or tested without hardware.

---

## State machine

```
         ┌──────────────────────────────┐
         ▼                              │
       Normal ──────full──────► Full    │
         │        ◄──normal────         │
         │                              │
         └──────low────────► Low        │
                   ◄──normal──          │
                                        │
         All states ──► Normal ─────────┘
```

```cpp
enum class TankStateMachineState : uint8_t {
    Normal = 0,
    Full   = 1,
    Low    = 2,
};
```

---

## Key types

### `LevelReadCallback` — injected sensor read function

```cpp
using LevelReadCallback = std::function<Result<uint16_t>()>;
```

Returns the current sensor reading in **millimetres**.

### `LevelAnalyticsCallback` — injected decision function

```cpp
using LevelAnalyticsCallback = std::function<
    TankStateMachineState(Span<const uint16_t>, int32_t full_mm, int32_t low_mm)
>;
```

The default is `level_analytics_basic_decision()` which averages the sample window.

### `TankMonitorEventCallback` — subscriber callback

```cpp
using TankMonitorEventCallback = std::function<void(EventType, int32_t)>;
```

---

## Public API (interface)

```cpp
class ITankMonitor {
public:
    virtual Result<void> init()         = 0;
    virtual Result<void> deinit()       = 0;
    virtual Result<void> check_level()  = 0;  // called by TankMonitorTask

    virtual Result<int32_t> add_subscriber(TankMonitorEventCallback cb)    = 0;
    virtual Result<void>    remove_subscriber(int32_t id)                  = 0;

    virtual int32_t id() const noexcept = 0;
};
```

### `TankMonitorConfig`

```cpp
struct TankMonitorConfig {
    int32_t                id{0};
    TankConfig             tank_config{};
    LevelReadCallback      read_cb{};
    int32_t                number_of_samples{1};
    LevelAnalyticsCallback analytics_cb{};  // defaults to level_analytics_basic_decision
};
```

---

## How `check_level()` works

1. Calls `read_cb()` to get the current level in mm.
2. Adds the reading to a circular sample buffer.
3. Calls `analytics_cb(samples, full_level_mm, low_level_mm)` to decide the new state.
4. If the state changed, fires `TankMonitorEventCallback` on all registered subscribers, mapping:
   - `TankStateMachineState::Normal` → `EventType::TankNormal`
   - `TankStateMachineState::Full`   → `EventType::TankFull`
   - `TankStateMachineState::Low`    → `EventType::TankLow`

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `void*` callback context | Lambda capture in `LevelReadCallback` |
| Raw `tank_t*` pointer in config | `TankConfig` held by value |
| `error_type_t` + `uint16_t*` output param | `Result<uint16_t>` |
| Separate `level_sensor_t*` + callback | Single `LevelReadCallback` |
| Fixed C array for samples | `Span<const uint16_t>` passed to analytics |
