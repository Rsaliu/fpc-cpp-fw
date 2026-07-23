# level_sensor

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `protocol`

---

## What it does

`level_sensor` provides the water-level sensing abstraction. It defines a pure interface (`ILevelSensor`) and a concrete implementation (`LevelSensor`) that talks to **GA1 ultrasonic distance sensors** over an **RS485 bus**.

Like `current_sensor`, the hardware interaction is fully injectable: three `std::function` callbacks — `FrameBuilder`, `Transport`, and `ResponseInterpreter` — are provided at construction time by `Factory`. This means `LevelSensor` can be unit-tested with stub callbacks, with no RS485 hardware required.

---

## Callback types

```cpp
// Builds an 8-byte Modbus RTU request frame for a given slave address
using FrameBuilder = std::function<protocol::gl_a01::RequestFrame(uint8_t)>;

// Sends request frame over RS485, receives response into a buffer
using Transport = std::function<Result<void>(
    ByteView request,
    MutableByteView response_buf,
    uint32_t timeout_ms,
    int32_t& bytes_read
)>;

// Decodes the raw response bytes into a 16-bit level value (mm)
using ResponseInterpreter = std::function<Result<uint16_t>(ByteView response)>;
```

---

## Public API

### `ILevelSensor` — pure interface

```cpp
class ILevelSensor {
public:
    virtual Result<void>     init()   = 0;
    virtual Result<void>     deinit() = 0;
    virtual Result<uint16_t> read()   = 0;  // returns level in mm
    virtual int32_t          id()     const noexcept = 0;
};
```

`tank_monitor` depends only on `ILevelSensor`.

---

### `LevelSensorConfig`

```cpp
struct LevelSensorConfig {
    int32_t             id{-1};
    uint8_t             address{1};         // Modbus slave address (1–247)
    uint32_t            timeout_ms{200};
    FrameBuilder        frame_builder{};
    Transport           transport{};
    ResponseInterpreter interpreter{};
};
```

---

### `LevelSensor`

```cpp
class LevelSensor final : public ILevelSensor {
public:
    explicit LevelSensor(LevelSensorConfig config);

    Result<void>     init()   override;
    Result<void>     deinit() override;
    Result<uint16_t> read()   override;
    int32_t          id()     const noexcept override;
};
```

`read()` calls `frame_builder(address)` → `transport(frame, buf, timeout, n)` → `interpreter(ByteView{buf, n})` and returns the resulting level in **millimetres**.

---

## Hardware pins (from `hardware_pins.hpp`)

| Signal | Pin |
|---|---|
| RS485 TX (DI) | GPIO 10 |
| RS485 RX (RO) | GPIO 11 |
| RS485 DIR (DE/RE) | GPIO 9 |
| UART port | UART1 |
| Baud rate | 9600 |

---

## GA1 sensor register map

| Register address | Value |
|---|---|
| `0x0100` | Level reading (mm) |
| `0x0102` | Temperature (0.1 °C) |

---

## Example — stub for testing

```cpp
LevelSensorConfig cfg;
cfg.id      = 1;
cfg.address = 1;
cfg.frame_builder  = [](uint8_t a){ return protocol::gl_a01::build_read_level(a); };
cfg.transport      = [](ByteView, MutableByteView buf, uint32_t, int32_t& n) {
    // simulate a response of 850 mm
    buf[0]=0x01; buf[1]=0x03; buf[2]=0x02;
    buf[3]=0x03; buf[4]=0x52;  // 0x0352 = 850
    buf[5]=0xXX; buf[6]=0xXX;  // CRC
    n = 7;
    return Result<void>::ok();
};
cfg.interpreter    = [](ByteView r){ return protocol::gl_a01::interpret_response(r); };

LevelSensor sensor{cfg};
sensor.init();
auto r = sensor.read();  // r.value() == 850
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `void*` callback context passed to frame builder | Lambda capture — no `void*` needed |
| `error_type_t` + `uint16_t*` output parameter | `Result<uint16_t>` — value and error bundled |
| Separate `rs485_t*` pointer threaded through every function | `Transport` callback captures the `Rs485` instance |
