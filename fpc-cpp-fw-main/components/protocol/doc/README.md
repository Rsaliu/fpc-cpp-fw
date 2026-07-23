# protocol

**Type:** Library  
**Namespace:** `fpc::protocol::gl_a01`  
**Depends on:** `common`, `crc`

---

## What it does

`protocol` is a **stateless Modbus RTU frame library** for the **GL-A01** ultrasonic level/temperature sensor. It provides free functions that build request frames (byte arrays ready to send over RS485) and parse response frames back into readable values.

It has no hardware dependencies and no state — it is purely a byte-manipulation utility used by `level_sensor`.

---

## Protocol overview

GL-A01 uses **Modbus RTU** framing:
- Requests and responses are 8 bytes and 7 bytes respectively.
- Each frame ends with a CRC-16/MODBUS checksum (computed by the `crc` component).
- The sensor exposes two registers: level (mm) at `0x0100` and temperature (0.1 °C) at `0x0102`.

---

## Public API

### Constants

```cpp
namespace fpc::protocol::gl_a01 {

constexpr std::size_t kRequestFrameSize  = 8u;
constexpr std::size_t kResponseFrameSize = 7u;

constexpr uint8_t  kFcReadHolding  = 0x03u;  // Read Holding Registers
constexpr uint8_t  kFcWriteSingle  = 0x06u;  // Write Single Register

constexpr uint16_t kRegLevel       = 0x0100u; // Level register
constexpr uint16_t kRegTemperature = 0x0102u; // Temperature register
constexpr uint16_t kRegAddress     = 0x0200u; // Slave address register

using RequestFrame = std::array<uint8_t, kRequestFrameSize>;
```

---

### Frame builder functions

```cpp
// Build a "Read Level" request frame for a given slave address
[[nodiscard]] RequestFrame build_read_level(uint8_t slave_addr);

// Build a "Read Temperature" request frame
[[nodiscard]] RequestFrame build_read_temperature(uint8_t slave_addr);

// Build a "Write Address" request frame (to change the sensor's slave address)
[[nodiscard]] RequestFrame build_write_address(uint8_t current_addr, uint8_t new_addr);
```

All functions return a fixed-size `std::array<uint8_t, 8>` **by value** — no output-pointer parameters, no caller-managed heap buffers.

---

### Response parser

```cpp
// Decode a raw 7-byte response frame into a 16-bit sensor value
[[nodiscard]] Result<uint16_t> interpret_response(ByteView response);
```

Returns:
- `ok(value)` — 16-bit register value (level in mm, or temperature × 10 in °C).
- `err(SystemError::InvalidLength)` — response is too short.
- `err(SystemError::ChecksumValidationFailed)` — CRC mismatch.
- `err(SystemError::InvalidResponse)` — unexpected function code.

---

## Wire format example (read level, slave 0x01)

**Request (8 bytes):**
```
01 03 01 00 00 01 [CRC_LO] [CRC_HI]
│  │  │──────┘ │───────┘
│  │  register  count=1
│  FC=Read Holding
slave address
```

**Response (7 bytes):**
```
01 03 02 [HI] [LO] [CRC_LO] [CRC_HI]
         └─────────┘
         2-byte register value = level in mm
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `void build_frame(uint8_t* out, uint8_t addr)` — output pointer | `RequestFrame build_read_level(uint8_t addr)` — returned by value |
| `#define FRAME_SIZE 8` | `constexpr std::size_t kRequestFrameSize = 8u` — typed |
| `int interpret(uint8_t* buf, uint16_t* out)` — two output params | `Result<uint16_t> interpret_response(ByteView)` — one typed return |
