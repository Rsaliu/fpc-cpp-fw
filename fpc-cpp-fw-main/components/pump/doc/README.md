# pump

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`pump` is the **pump data model**. It represents a single physical pump as a C++ class with configuration, runtime state, and a simple lifecycle state machine. It does **not** drive any GPIO — relay control is handled by `relay_driver`. Pump monitors observe this object and instruct it to change state.

---

## State machine

```
NotInitialized ──init()──► Initialized ──(monitor sets)──► On
                                ▲                           │
                                └──────────(monitor)────────┘
              deinit() returns to NotInitialized from any state
```

```cpp
enum class PumpState : uint8_t {
    NotInitialized = 0,
    Initialized    = 1,
    On             = 2,
    Off            = 3,
};
```

---

## Public API

### `PumpConfig`

```cpp
struct PumpConfig {
    int32_t     id{-1};
    std::string make{};               // e.g. "TestPump"
    float       power_in_hp{0.0f};
    float       current_rating{0.0f};      // rated operating current (A) — overcurrent threshold
    float       min_working_current{0.0f}; // undercurrent threshold (A)
};
```

---

### `Pump`

```cpp
class Pump {
public:
    explicit Pump(PumpConfig config);

    Result<void> init();
    Result<void> deinit();

    Result<void> turn_on();
    Result<void> turn_off();

    [[nodiscard]] PumpState  state()           const noexcept;
    [[nodiscard]] int32_t    id()              const noexcept;
    [[nodiscard]] float      current_rating()  const noexcept;
    [[nodiscard]] float      min_working_current() const noexcept;
    [[nodiscard]] std::string_view make()      const noexcept;
};
```

`turn_on()` / `turn_off()` update the state machine. They do **not** drive hardware — they simply record that the pump was commanded on or off, so monitors and control units can query the current state.

---

## Thresholds used by `PumpMonitor`

| Field | Meaning |
|---|---|
| `current_rating` | If measured current exceeds this → `PumpOvercurrent` event |
| `min_working_current` | If measured current falls below this → `PumpUndercurrent` event |

These values come from `config.json` and are set once at construction time via `PumpConfig`.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `pump_state_t` plain C enum | `enum class PumpState : uint8_t` — scoped |
| `char* make` raw pointer | `std::string make` — owned |
| `error_type_t pump_init(pump_t*)` | `Result<void> Pump::init()` |
| Global `pump_t` struct with manual state management | Non-copyable `Pump` class with lifecycle methods |
