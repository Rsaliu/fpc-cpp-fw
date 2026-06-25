/**
 * @file crc.cpp
 * @brief CRC-16/MODBUS implementation.
 *
 * The public API (including the function template for std::array) is defined
 * in crc.hpp.  This file provides the primary runtime implementation that
 * operates on a `fpc::ByteView` (fpc::Span<const uint8_t>).
 */

#include "crc.hpp"
#include "esp_log.h"

static constexpr char TAG[] = "CRC";

namespace fpc::crc {

// ─────────────────────────────────────────────────────────────────────────
// crc16_modbus — primary implementation
// ─────────────────────────────────────────────────────────────────────────

uint16_t crc16_modbus(fpc::ByteView data) noexcept
{
    uint16_t crc = 0xFFFFu;

    for (const uint8_t byte : data) {
        // XOR the low-byte of the running CRC with the incoming byte.
        const uint8_t idx = static_cast<uint8_t>(
            byte ^ static_cast<uint8_t>(crc & 0xFFu));
        crc = static_cast<uint16_t>((crc >> 8u) ^ kCrc16Table[idx]);
    }

    ESP_LOGV(TAG, "crc16_modbus over %zu bytes → 0x%04X", data.size(), crc);
    return crc;
}

// ─────────────────────────────────────────────────────────────────────────
// validate_frame_crc
// ─────────────────────────────────────────────────────────────────────────

bool validate_frame_crc(fpc::ByteView frame) noexcept
{
    // Need at least 1 payload byte + 2 CRC bytes.
    if (frame.size() < 3u) {
        ESP_LOGW(TAG, "validate_frame_crc: frame too short (%zu bytes)",
                 frame.size());
        return false;
    }

    // Payload is everything except the last two bytes.
    const fpc::ByteView payload{frame.data(), frame.size() - 2u};
    const uint16_t computed = crc16_modbus(payload);

    const uint8_t lo_expected = frame[frame.size() - 2u];
    const uint8_t hi_expected = frame[frame.size() - 1u];
    const uint16_t expected   =
        static_cast<uint16_t>(lo_expected | (static_cast<uint16_t>(hi_expected) << 8u));

    const bool valid = (computed == expected);
    if (!valid) {
        ESP_LOGW(TAG,
                 "CRC mismatch: computed=0x%04X  expected=0x%04X",
                 computed, expected);
    }
    return valid;
}

} // namespace fpc::crc
