# current_sensor

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `hardware_pins` (via `common`)

---

## What it does

`current_sensor` provides the current-sensing abstraction used to monitor pump health. It defines a pure interface (`ICurrentSensor`) and a concrete implementation (`CurrentSensor`) whose hardware-reading logic is fully injected via a `std::function` callback.

The concrete sensor supported is the **ACS712** hall-effect current sensor, readable through two different hardware paths:
- **ADS1115** external I²C ADC (preferred for accuracy)
- **ESP32-S3 internal ADC** (fallback)

Because the read function is injected at construction time, the same `CurrentSensor` class can be used in unit tests with a simple lambda stub — no hardware required.

---

## Key types

### `ReadCallback`

```cpp
using ReadCallback = std::function<Result<float>()>;
```

A zero-argument callable that returns the current reading in **Amperes**, or an error. All hardware interaction lives inside this callback (injected from `Factory`).

---

### `ICurrentSensor` — pure interface

```cpp
class ICurrentSensor {
public:
    virtual Result<void>  init()   = 0;
    virtual Result<void>  deinit() = 0;
    virtual Result<float> read()   = 0;
    virtual int32_t       id()     const noexcept = 0;
};
```

`pump_monitor` depends only on `ICurrentSensor`, never on the concrete class.

---

### `CurrentSensorConfig`

```cpp
struct CurrentSensorConfig {
    int32_t     id{-1};
    std::string make{};        // e.g. "ACS712"
    ReadCallback read_cb{};    // injected read function
};
```

---

### `CurrentSensor`

```cpp
class CurrentSensor final : public ICurrentSensor {
public:
    explicit CurrentSensor(CurrentSensorConfig config);

    Result<void>  init()   override;
    Result<void>  deinit() override;
    Result<float> read()   override;  // delegates to read_cb
    int32_t       id()     const noexcept override;
};
```

---

## Hardware pin assignments

Defined in `hardware_pins.hpp` (part of `common`):

| Signal | Pin |
|---|---|
| ADS1115 SDA | GPIO 6 |
| ADS1115 SCL | GPIO 7 |
| I²C address | 0x48 |
| I²C speed | 100 kHz |

---

## Example — test stub

```cpp
CurrentSensorConfig cfg;
cfg.id   = 1;
cfg.make = "StubSensor";
cfg.read_cb = []() -> Result<float> {
    return Result<float>::ok(5.0f);  // always returns 5A
};
CurrentSensor sensor{cfg};
sensor.init();
auto r = sensor.read();
// r.value() == 5.0f
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `void*` callback context passed alongside function pointer | Lambda capture — context lives inside the `std::function` |
| `error_type_t read_current(sensor*, float* out)` | `Result<float> read()` — value and error bundled |
| `char* make` — raw pointer | `std::string make` — owned string, no lifetime issues |
| Read mode (`basic`/`continuous`/`overcurrent`) handled by internal `switch` | Different `ReadCallback` implementations — no branching inside the sensor class |
