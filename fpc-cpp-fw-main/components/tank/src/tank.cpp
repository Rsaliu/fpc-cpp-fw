/**
 * @file tank.cpp
 * @brief Tank class implementation.
 */

#include "tank.hpp"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

static constexpr char TAG[] = "TANK";

namespace fpc {

// ─────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────

Tank::Tank(TankConfig config) noexcept
    : m_config{std::move(config)}
{}

// ─────────────────────────────────────────────────────────────────────────
// init
// ─────────────────────────────────────────────────────────────────────────

Result<void> Tank::init()
{
    if (m_config.id < 0 ||
        m_config.capacity_litres <= 0.0f ||
        m_config.height_cm <= 0.0f ||
        m_config.full_level_mm < 0 ||
        m_config.low_level_mm  < 0 ||
        m_config.full_level_mm <= m_config.low_level_mm)
    {
        ESP_LOGE(TAG, "[id=%ld] init failed: invalid config", m_config.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }

    if (m_state != TankState::NotInitialized) {
        ESP_LOGW(TAG, "[id=%ld] init: already initialised", m_config.id);
        return Result<void>::err(SystemError::InvalidState);
    }

    m_state = TankState::Initialized;
    ESP_LOGI(TAG, "[id=%ld cap=%.2fL shape=%s] initialised",
             m_config.id, m_config.capacity_litres,
             to_string(m_config.shape).data());
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// deinit
// ─────────────────────────────────────────────────────────────────────────

Result<void> Tank::deinit()
{
    if (m_state == TankState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_state = TankState::NotInitialized;
    ESP_LOGI(TAG, "[id=%ld] deinitialised", m_config.id);
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────

TankState Tank::get_state() const noexcept
{
    return m_state;
}

const TankConfig& Tank::get_config() const noexcept
{
    return m_config;
}

// ─────────────────────────────────────────────────────────────────────────
// format_info
// ─────────────────────────────────────────────────────────────────────────

std::string Tank::format_info() const
{
    std::array<char, 256> buf{};
    std::snprintf(buf.data(), buf.size(),
        "Tank ID: %ld\n"
        " Capacity: %.2f liters\n"
        " Height: %.2f cm\n"
        " Low Level: %ld mm\n"
        " High Level: %ld mm\n"
        " Shape: %s\n",
        m_config.id,
        m_config.capacity_litres,
        m_config.height_cm,
        m_config.low_level_mm,
        m_config.full_level_mm,
        to_string(m_config.shape).data());
    return std::string{buf.data()};
}

// ─────────────────────────────────────────────────────────────────────────
// format_info_into
// ─────────────────────────────────────────────────────────────────────────

Result<void> Tank::format_info_into(MutableByteView buf) const
{
    if (buf.empty()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    const std::string info = format_info();
    if (info.size() + 1u > buf.size()) {
        std::memcpy(buf.data(), info.data(), buf.size() - 1u);
        buf[buf.size() - 1u] = '\0';
        return Result<void>::err(SystemError::BufferOverflow);
    }

    std::memcpy(buf.data(), info.data(), info.size() + 1u);
    return Result<void>::ok();
}

} // namespace fpc
