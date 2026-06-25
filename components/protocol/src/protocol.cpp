/**
 * @file protocol.cpp
 * @brief GL-A01 Modbus RTU protocol implementation.
 */

#include "protocol.hpp"
#include "esp_log.h"

static constexpr char TAG[] = "PROTOCOL";

namespace fpc::protocol::gl_a01 {

// ── Internal helper: append CRC to the first 6 bytes of a frame ──────────

static void append_crc(RequestFrame& frame) noexcept
{
    const ByteView payload{frame.data(), kRequestFrameSize - 2u};
    const uint16_t crc = crc::crc16_modbus(payload);
    frame[6] = static_cast<uint8_t>(crc & 0xFFu);
    frame[7] = static_cast<uint8_t>((crc >> 8u) & 0xFFu);
}

// ═══════════════════════════════════════════════════════════════════════════
// build_write_address
// ═══════════════════════════════════════════════════════════════════════════

RequestFrame build_write_address(uint8_t current_addr, uint8_t new_addr) noexcept
{
    RequestFrame frame{};
    frame[0] = current_addr;
    frame[1] = kFcWriteSingle;
    frame[2] = static_cast<uint8_t>((kRegAddress >> 8u) & 0xFFu); // 0x02
    frame[3] = static_cast<uint8_t>(kRegAddress & 0xFFu);          // 0x00
    frame[4] = 0x00u;
    frame[5] = new_addr;
    append_crc(frame);
    ESP_LOGD(TAG, "build_write_address: slave=0x%02X new=0x%02X", current_addr, new_addr);
    return frame;
}

// ═══════════════════════════════════════════════════════════════════════════
// build_read_level
// ═══════════════════════════════════════════════════════════════════════════

RequestFrame build_read_level(uint8_t slave_addr) noexcept
{
    RequestFrame frame{};
    frame[0] = slave_addr;
    frame[1] = kFcReadHolding;
    frame[2] = static_cast<uint8_t>((kRegLevel >> 8u) & 0xFFu);  // 0x01
    frame[3] = static_cast<uint8_t>(kRegLevel & 0xFFu);           // 0x00
    frame[4] = static_cast<uint8_t>((kRegCount1 >> 8u) & 0xFFu);  // 0x00
    frame[5] = static_cast<uint8_t>(kRegCount1 & 0xFFu);          // 0x01
    append_crc(frame);
    ESP_LOGD(TAG, "build_read_level: slave=0x%02X", slave_addr);
    return frame;
}

// ═══════════════════════════════════════════════════════════════════════════
// build_read_temp
// ═══════════════════════════════════════════════════════════════════════════

RequestFrame build_read_temp(uint8_t slave_addr) noexcept
{
    RequestFrame frame{};
    frame[0] = slave_addr;
    frame[1] = kFcReadHolding;
    frame[2] = static_cast<uint8_t>((kRegTemperature >> 8u) & 0xFFu); // 0x01
    frame[3] = static_cast<uint8_t>(kRegTemperature & 0xFFu);          // 0x02
    frame[4] = static_cast<uint8_t>((kRegCount1 >> 8u) & 0xFFu);       // 0x00
    frame[5] = static_cast<uint8_t>(kRegCount1 & 0xFFu);               // 0x01
    append_crc(frame);
    ESP_LOGD(TAG, "build_read_temp: slave=0x%02X", slave_addr);
    return frame;
}

// ═══════════════════════════════════════════════════════════════════════════
// interpret_response
// ═══════════════════════════════════════════════════════════════════════════

Result<uint16_t> interpret_response(ByteView frame) noexcept
{
    if (frame.size() < kResponseFrameSize) {
        ESP_LOGE(TAG, "interpret_response: frame too short (%zu bytes)", frame.size());
        return Result<uint16_t>::err(SystemError::InvalidLength);
    }

    // CRC covers everything except the last two bytes.
    const ByteView payload{frame.data(), frame.size() - 2u};
    const uint16_t computed  = crc::crc16_modbus(payload);
    const uint16_t received  = static_cast<uint16_t>(
        frame[frame.size() - 2u] |
        (static_cast<uint16_t>(frame[frame.size() - 1u]) << 8u));

    if (computed != received) {
        ESP_LOGE(TAG, "interpret_response: CRC mismatch computed=0x%04X received=0x%04X",
                 computed, received);
        return Result<uint16_t>::err(SystemError::ChecksumValidationFailed);
    }

    const uint16_t sensor_data =
        static_cast<uint16_t>(
            (static_cast<uint16_t>(frame[3]) << 8u) | frame[4]);

    ESP_LOGI(TAG, "interpret_response: sensor_data=%u (0x%04X)", sensor_data, sensor_data);
    return Result<uint16_t>::ok(sensor_data);
}

} // namespace fpc::protocol::gl_a01
