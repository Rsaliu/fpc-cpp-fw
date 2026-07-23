/**
 * @file level_sensor.cpp
 * @brief LevelSensor concrete implementation.
 */

#include "level_sensor.hpp"
#include "esp_log.h"
#include <array>

static constexpr char TAG[] = "LEVEL_SENSOR";

namespace fpc {

// ─────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────

LevelSensor::LevelSensor(LevelSensorConfig config) noexcept
    : m_config{std::move(config)}
{}

// ─────────────────────────────────────────────────────────────────────────
// init
// ─────────────────────────────────────────────────────────────────────────

Result<void> LevelSensor::init()
{
    if (!m_config.frame_builder ||
        !m_config.transport     ||
        !m_config.interpreter)
    {
        ESP_LOGE(TAG, "[id=%ld] init: one or more callbacks are null", m_config.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }

    if (m_active) {
        return Result<void>::err(SystemError::InvalidState);
    }

    m_active = true;
    ESP_LOGI(TAG, "[id=%ld addr=0x%02X] initialised",
             m_config.id, m_config.sensor_addr);
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// deinit
// ─────────────────────────────────────────────────────────────────────────

Result<void> LevelSensor::deinit()
{
    if (!m_active) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_active = false;
    ESP_LOGI(TAG, "[id=%ld] deinitialised", m_config.id);
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// read
// ─────────────────────────────────────────────────────────────────────────

Result<uint16_t> LevelSensor::read()
{
    if (!m_active) {
        return Result<uint16_t>::err(SystemError::InvalidState);
    }

    // 1. Build request frame.
    const auto request = m_config.frame_builder(m_config.sensor_addr);
    const ByteView request_view{request.data(), request.size()};

    // 2. Send request, receive response.
    std::array<uint8_t, kRxBufSize> rx_buf{};
    int32_t bytes_read{0};

    auto tx_result = m_config.transport(request_view,
                                        MutableByteView{rx_buf},
                                        m_config.timeout_ms,
                                        bytes_read);
    if (tx_result.is_err()) {
        ESP_LOGE(TAG, "[id=%ld] transport failed: %s",
                 m_config.id, to_string(tx_result.error()).data());
        return Result<uint16_t>::err(tx_result.error());
    }

    if (bytes_read == 0) {
        ESP_LOGE(TAG, "[id=%ld] no response from sensor", m_config.id);
        return Result<uint16_t>::err(SystemError::NoResponse);
    }

    // 3. Interpret response.
    const ByteView response{rx_buf.data(), static_cast<std::size_t>(bytes_read)};
    auto interp = m_config.interpreter(response);
    if (interp.is_err()) {
        ESP_LOGE(TAG, "[id=%ld] interpreter failed: %s",
                 m_config.id, to_string(interp.error()).data());
        return interp;
    }

    ESP_LOGI(TAG, "[id=%ld] level=%u mm", m_config.id, interp.value());
    return interp;
}

// ─────────────────────────────────────────────────────────────────────────
// id
// ─────────────────────────────────────────────────────────────────────────

int32_t LevelSensor::id() const
{
    return m_config.id;
}

} // namespace fpc
