# crc

**Type:** Library  
**Namespace:** `fpc::crc`  
**Depends on:** `common`

---

## What it does

`crc` provides a **CRC-16/MODBUS** checksum implementation used to validate RS485 message frames exchanged with GA1 level sensors. The polynomial is `0x8005` (reflected representation: `0xA001`).

This is a pure utility component — no state, no hardware, no FreeRTOS. It is used by the `protocol` component to append and verify CRCs on every Modbus RTU frame.

---

## Public API

### From a `ByteView` (runtime data)

```cpp
namespace fpc::crc {

[[nodiscard]] uint16_t crc16_modbus(ByteView data) noexcept;

}
```

**Example:**

```cpp
fpc::Bytes payload = {0x01, 0x03, 0x01, 0x02, 0x00, 0x01};
uint16_t checksum = fpc::crc::crc16_modbus(fpc::ByteView{payload});
```

---

### From a `std::array` (compile-time capable)

```cpp
template<std::size_t N>
[[nodiscard]] uint16_t crc16_modbus(const std::array<uint8_t, N>& data) noexcept;
```

**Example:**

```cpp
std::array<uint8_t, 6> frame = {0x01, 0x03, 0x01, 0x02, 0x00, 0x01};
uint16_t cs = fpc::crc::crc16_modbus(frame);  // N deduced as 6
```

---

### Lookup table

```cpp
inline constexpr std::array<uint16_t, 256> kCrc16Table = { /* 256 pre-computed values */ };
```

The 256-entry lookup table is `inline constexpr`, meaning:
- Only one copy exists across all translation units.
- It lives in **read-only flash** at link time.
- It can be used in `constexpr` contexts.

---

## Algorithm

CRC-16/MODBUS uses a table-driven approach: for each input byte, XOR it with the low byte of the running CRC, use the result as an index into `kCrc16Table`, and XOR with the shifted CRC. This runs in O(n) time and requires no division or reflection operations at runtime.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `uint16_t crc16(uint8_t* data, uint16_t length)` — raw pointer + length | `crc16_modbus(ByteView)` — non-owning view, no pointer arithmetic |
| `static const uint16_t crc_table[256]` | `inline constexpr std::array<uint16_t, 256> kCrc16Table` — type-safe, constexpr |
| Template overload did not exist | `crc16_modbus(std::array<uint8_t, N>)` template — zero overhead, N deduced |
