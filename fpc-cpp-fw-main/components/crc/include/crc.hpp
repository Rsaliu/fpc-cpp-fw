/**
 * @file crc.hpp
 * @brief CRC-16/MODBUS computation — C++17 pure-function interface.
 *
 * Design principles applied from fresh_cpp_project_prompt.md:
 *  - Primary function takes `fpc::ByteView` (alias for `fpc::Span<const uint8_t>`).
 *  - Convenience function template for `std::array<uint8_t, N>` — resolved fully
 *    at compile time, zero overhead.
 *  - Lookup table declared `inline constexpr` — lives in read-only flash.
 *  - No global mutable state; all functions are `[[nodiscard]]`.
 *  - No C-style arrays, no raw pointers in the public API.
 *
 * Usage:
 * @code
 *   using namespace fpc;
 *
 *   // From a vector / ByteView:
 *   Bytes payload = {0x01, 0x03, 0x01, 0x02, 0x00, 0x01};
 *   uint16_t checksum = crc::crc16_modbus(ByteView{payload});
 *
 *   // From a fixed-size array (template overload — constexpr-capable):
 *   std::array<uint8_t, 6> frame = {0x01, 0x03, 0x01, 0x02, 0x00, 0x01};
 *   uint16_t cs2 = crc::crc16_modbus(frame);  // deduced N = 6
 * @endcode
 */

#pragma once

#include <cstdint>
#include <array>
#include "common.hpp"

namespace fpc::crc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Lookup table (CRC-16/MODBUS, poly 0x8005 reflected = 0xA001)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Pre-computed CRC-16/MODBUS lookup table.
 *
 * Declared `inline constexpr` so:
 *  - Only one definition exists across all translation units (inline).
 *  - The array is placed in read-only flash at link time (constexpr).
 *  - Individual entries can be used in other constexpr contexts.
 */
inline constexpr std::array<uint16_t, 256> kCrc16Table = {
    0x0000u, 0xC0C1u, 0xC181u, 0x0140u, 0xC301u, 0x03C0u, 0x0280u, 0xC241u,
    0xC601u, 0x06C0u, 0x0780u, 0xC741u, 0x0500u, 0xC5C1u, 0xC481u, 0x0440u,
    0xCC01u, 0x0CC0u, 0x0D80u, 0xCD41u, 0x0F00u, 0xCFC1u, 0xCE81u, 0x0E40u,
    0x0A00u, 0xCAC1u, 0xCB81u, 0x0B40u, 0xC901u, 0x09C0u, 0x0880u, 0xC841u,
    0xD801u, 0x18C0u, 0x1980u, 0xD941u, 0x1B00u, 0xDBc1u, 0xDA81u, 0x1A40u,
    0x1E00u, 0xDEC1u, 0xDF81u, 0x1F40u, 0xDD01u, 0x1DC0u, 0x1C80u, 0xDC41u,
    0x1400u, 0xD4C1u, 0xD581u, 0x1540u, 0xD701u, 0x17C0u, 0x1680u, 0xD641u,
    0xD201u, 0x12C0u, 0x1380u, 0xD341u, 0x1100u, 0xD1C1u, 0xD081u, 0x1040u,
    0xF001u, 0x30C0u, 0x3180u, 0xF141u, 0x3300u, 0xF3C1u, 0xF281u, 0x3240u,
    0x3600u, 0xF6C1u, 0xF781u, 0x3740u, 0xF501u, 0x35C0u, 0x3480u, 0xF441u,
    0x3C00u, 0xFCC1u, 0xFD81u, 0x3D40u, 0xFF01u, 0x3FC0u, 0x3E80u, 0xFE41u,
    0xFA01u, 0x3AC0u, 0x3B80u, 0xFB41u, 0x3900u, 0xF9C1u, 0xF881u, 0x3840u,
    0x2800u, 0xE8C1u, 0xE981u, 0x2940u, 0xEB01u, 0x2BC0u, 0x2A80u, 0xEA41u,
    0xEE01u, 0x2EC0u, 0x2F80u, 0xEF41u, 0x2D00u, 0xEDC1u, 0xEC81u, 0x2C40u,
    0xE401u, 0x24C0u, 0x2580u, 0xE541u, 0x2700u, 0xE7C1u, 0xE681u, 0x2640u,
    0x2200u, 0xE2C1u, 0xE381u, 0x2340u, 0xE101u, 0x21C0u, 0x2080u, 0xE041u,
    0xA001u, 0x60C0u, 0x6180u, 0xA141u, 0x6300u, 0xA3C1u, 0xA281u, 0x6240u,
    0x6600u, 0xA6C1u, 0xA781u, 0x6740u, 0xA501u, 0x65C0u, 0x6480u, 0xA441u,
    0x6C00u, 0xACc1u, 0xAD81u, 0x6D40u, 0xAF01u, 0x6FC0u, 0x6E80u, 0xAE41u,
    0xAA01u, 0x6AC0u, 0x6B80u, 0xAB41u, 0x6900u, 0xA9C1u, 0xA881u, 0x6840u,
    0x7800u, 0xB8C1u, 0xB981u, 0x7940u, 0xBB01u, 0x7BC0u, 0x7A80u, 0xBA41u,
    0xBE01u, 0x7EC0u, 0x7F80u, 0xBF41u, 0x7D00u, 0xBDC1u, 0xBC81u, 0x7C40u,
    0xB401u, 0x74C0u, 0x7580u, 0xB541u, 0x7700u, 0xB7C1u, 0xB681u, 0x7640u,
    0x7200u, 0xB2C1u, 0xB381u, 0x7340u, 0xB101u, 0x71C0u, 0x7080u, 0xB041u,
    0x5000u, 0x90C1u, 0x9181u, 0x5140u, 0x9301u, 0x53C0u, 0x5280u, 0x9241u,
    0x9601u, 0x56C0u, 0x5780u, 0x9741u, 0x5500u, 0x95C1u, 0x9481u, 0x5440u,
    0x9C01u, 0x5CC0u, 0x5D80u, 0x9D41u, 0x5F00u, 0x9FC1u, 0x9E81u, 0x5E40u,
    0x5A00u, 0x9AC1u, 0x9B81u, 0x5B40u, 0x9901u, 0x59C0u, 0x5880u, 0x9841u,
    0x8801u, 0x48C0u, 0x4980u, 0x8941u, 0x4B00u, 0x8BC1u, 0x8A81u, 0x4A40u,
    0x4E00u, 0x8EC1u, 0x8F81u, 0x4F40u, 0x8D01u, 0x4DC0u, 0x4C80u, 0x8C41u,
    0x4400u, 0x84C1u, 0x8581u, 0x4540u, 0x8701u, 0x47C0u, 0x4680u, 0x8641u,
    0x8201u, 0x42C0u, 0x4380u, 0x8341u, 0x4100u, 0x81C1u, 0x8081u, 0x4040u,
};

// ═══════════════════════════════════════════════════════════════════════════
// § 2  CRC-16/MODBUS computation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Compute CRC-16/MODBUS over a contiguous byte span.
 *
 * This is the primary implementation — takes a non-owning `fpc::ByteView`
 * (i.e. `fpc::Span<const uint8_t>`) so the caller can pass any contiguous
 * buffer without copying.
 *
 * @param data  Read-only view over the bytes to checksum.
 * @return      16-bit CRC. Little-endian byte order: lo = result & 0xFF,
 *              hi = result >> 8.
 */
[[nodiscard]] uint16_t crc16_modbus(fpc::ByteView data) noexcept;

// ═══════════════════════════════════════════════════════════════════════════
// § 3  Function template — fixed-size std::array overload
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Function template: CRC-16/MODBUS over a fixed-size `std::array`.
 *
 * The array size @p N is deduced automatically, so callers write simply:
 * @code
 *   std::array<uint8_t, 6> frame = {0x01, 0x03, 0x01, 0x02, 0x00, 0x01};
 *   uint16_t crc = fpc::crc::crc16_modbus(frame);
 * @endcode
 *
 * The call delegates to the span overload — no data copies, no heap usage.
 *
 * @tparam N  Number of bytes (deduced).
 * @param arr Reference to the fixed-size byte array.
 * @return    16-bit CRC value.
 */
template<std::size_t N>
[[nodiscard]] inline uint16_t crc16_modbus(const std::array<uint8_t, N>& arr) noexcept
{
    return crc16_modbus(fpc::ByteView{arr.data(), arr.size()});
}

/**
 * @brief Validate a Modbus frame: compute CRC of @p payload and compare to
 *        the two appended CRC bytes (lo-byte first, then hi-byte).
 *
 * @param frame  Full frame including the two trailing CRC bytes.
 *               Must be at least 3 bytes (1 payload + 2 CRC).
 * @return       `true` if the computed CRC matches the appended bytes.
 */
[[nodiscard]] bool validate_frame_crc(fpc::ByteView frame) noexcept;

} // namespace fpc::crc
