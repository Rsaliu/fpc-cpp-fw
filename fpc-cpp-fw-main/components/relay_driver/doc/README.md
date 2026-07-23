# relay_driver

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`relay_driver` provides a **RAII GPIO relay abstraction** with a clean two-level interface hierarchy:

1. `IGpioDriver` — abstracts the raw ESP-IDF `gpio_*` calls. Swapped for a `MockGpioDriver` in unit tests.
2. `IRelay` — the public relay interface that higher-level components (`event_handler_task`, subscriptions) depend on.

This means relay behaviour can be tested end-to-end without real hardware.

---

## State machine

```
Off ──on()──► On ──trip()──► Tripped
 ▲             │
 └──off()──────┘
```

```cpp
enum class RelayState : uint8_t {
    Off     = 0,  // de-energised (GPIO LOW)
    On      = 1,  // energised (GPIO HIGH)
    Tripped = 2,  // fault — locked out until reset
};
```

---

## Public API

### `IGpioDriver` — hardware abstraction

```cpp
class IGpioDriver {
public:
    virtual Result<void> configure_output(gpio_num_t pin) = 0;
    virtual Result<void> set_level(gpio_num_t pin, uint32_t level) = 0;
    virtual Result<int>  get_level(gpio_num_t pin)                 = 0;
};
```

Production: `EspGpioDriver` calls real ESP-IDF `gpio_set_level()`.  
Tests: `MockGpioDriver` records calls and returns configurable results.

---

### `RelayConfig`

```cpp
struct RelayConfig {
    int32_t    id;   // application-level relay identifier (>= 0)
    gpio_num_t pin;  // GPIO pin driving the relay coil
};
```

---

### `IRelay` — public relay interface

```cpp
class IRelay {
public:
    virtual Result<void>       init()   = 0;
    virtual Result<void>       deinit() = 0;
    virtual Result<void>       on()     = 0;
    virtual Result<void>       off()    = 0;
    virtual Result<void>       trip()   = 0;   // fault lockout
    virtual Result<RelayState> state()  = 0;
    virtual int32_t            id()     const noexcept = 0;
};
```

---

### `Relay` — concrete implementation

```cpp
class Relay final : public IRelay {
public:
    Relay(RelayConfig config, IGpioDriver& gpio);
    // ...implements all IRelay methods
};
```

`on()` calls `gpio_.set_level(pin, 1)` and updates state.  
`off()` calls `gpio_.set_level(pin, 0)` and updates state.  
`trip()` calls `off()` and locks state to `Tripped` until `reset()` is called.

---

## Example

```cpp
EspGpioDriver gpio_driver;
Relay relay{RelayConfig{.id = 1, .pin = GPIO_NUM_4}, gpio_driver};

relay.init();
relay.on();   // GPIO 4 → HIGH
relay.off();  // GPIO 4 → LOW

// In a test:
MockGpioDriver mock;
Relay test_relay{RelayConfig{.id = 1, .pin = GPIO_NUM_4}, mock};
test_relay.on();
TEST_ASSERT_EQUAL(1, mock.last_level());
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Direct `gpio_set_level()` calls in relay code | `IGpioDriver` abstraction — mockable |
| `relay_state_t` plain enum | `enum class RelayState : uint8_t` — scoped |
| `error_type_t relay_on(relay_t*)` | `Result<void> Relay::on()` |
| No fault/trip concept | `trip()` + `Tripped` state |
