/**
 * @file protocol.hpp
 * @brief GL-A01 level/temperature sensor Modbus RTU frame encoder/decoder.
 *
 * This component is a stateless frame-building and response-parsing library.
 * It depends only on `crc` (for CRC-16/MODBUS) and `common` (for types).
 *
 * C++17 design:
 *  - All public functions are free functions in `fpc::protocol::gl_a01`.
 *  - Request frames are returned as `std::array<uint8_t, kFrameSize>` by
 *    value — no output-pointer parameters, no caller-managed buffers.
 *  - `interpret_response()` returns `Result<uint16_t>` — no out-parameter.
 *  - Modbus constants are `constexpr`, not `#define`.
 *  - CRC is computed via `fpc::crc::crc16_modbus(ByteView)`.
 *
 * GL-A01 Modbus RTU register map (relative to base address 0x0100):
 *  - 0x0100 : Level reading (mm)
 *  - 0x0102 : Temperature reading (0.1 °C)
 */

#pragma once

#include <cstdint>
#include <array>
#include "common.hpp"
#include "crc.hpp"

namespace fpc::protocol::gl_a01 {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Protocol constants
// ═══════════════════════════════════════════════════════════════════════════

/// All GL-A01 request / response frames are 8 bytes (request) or 7 bytes (response).
inline constexpr std::size_t kRequestFrameSize  = 8u;
inline constexpr std::size_t kResponseFrameSize = 7u;  ///< Minimum response size.

/// Modbus function codes used by GL-A01.
inline constexpr uint8_t kFcReadHolding  = 0x03u; ///< Read Holding Registers.
inline constexpr uint8_t kFcWriteSingle  = 0x06u; ///< Write Single Register.

/// GL-A01 register addresses (16-bit, big-endian on wire).
inline constexpr uint16_t kRegLevel       = 0x0100u; ///< Level reading register.
inline constexpr uint16_t kRegTemperature = 0x0102u; ///< Temperature register.
inline constexpr uint16_t kRegAddress     = 0x0200u; ///< Device address register.

/// Number of registers to read for a single-register request.
inline constexpr uint16_t kRegCount1     = 0x0001u;

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Frame type alias
// ═══════════════════════════════════════════════════════════════════════════

/// A fixed-size 8-byte Modbus RTU request frame.
using RequestFrame = std::array<uint8_t, kRequestFrameSize>;

// ═══════════════════════════════════════════════════════════════════════════
// § 3  Frame builder functions
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Build a Modbus "Write Single Register" frame to change the sensor's
 *        slave address.
 *
 * Frame layout:
 *   [current_addr][0x06][0x02][0x00][0x00][new_addr][CRC_LO][CRC_HI]
 *
 * @param current_addr  Present slave address of the sensor (0x01–0xF7).
 * @param new_addr      Desired new slave address.
 * @return              8-byte request frame with CRC appended.
 */
[[nodiscard]] RequestFrame build_write_address(uint8_t current_addr,
                                               uint8_t new_addr) noexcept;

/**
 * @brief Build a Modbus "Read Holding Registers" frame for the level register.
 *
 * Frame layout:
 *   [slave_addr][0x03][0x01][0x00][0x00][0x01][CRC_LO][CRC_HI]
 *
 * @param slave_addr  Slave address of the GL-A01 sensor.
 * @return            8-byte request frame with CRC appended.
 */
[[nodiscard]] RequestFrame build_read_level(uint8_t slave_addr) noexcept;

/**
 * @brief Build a Modbus "Read Holding Registers" frame for the temperature register.
 *
 * Frame layout:
 *   [slave_addr][0x03][0x01][0x02][0x00][0x01][CRC_LO][CRC_HI]
 *
 * @param slave_addr  Slave address of the GL-A01 sensor.
 * @return            8-byte request frame with CRC appended.
 */
[[nodiscard]] RequestFrame build_read_temp(uint8_t slave_addr) noexcept;

// ═══════════════════════════════════════════════════════════════════════════
// § 4  Response interpreter
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Validate and decode a GL-A01 single-register response frame.
 *
 * Expected response layout (7 bytes for 1-register read):
 *   [slave][0x03][0x02][data_hi][data_lo][CRC_LO][CRC_HI]
 *
 * Steps:
 *   1. Verify @p frame is at least `kResponseFrameSize` bytes.
 *   2. Compute CRC of the first (size-2) bytes; compare with the last 2.
 *   3. Extract `(frame[3] << 8) | frame[4]` as the 16-bit sensor value.
 *
 * @param frame  Complete response frame including CRC bytes.
 * @return       `Result<uint16_t>` — the raw sensor reading on success,
 *               or `SystemError::InvalidLength` if too short,
 *               or `SystemError::ChecksumValidationFailed` on CRC mismatch.
 */
[[nodiscard]] Result<uint16_t> interpret_response(ByteView frame) noexcept;

} // namespace fpc::protocol::gl_a01
