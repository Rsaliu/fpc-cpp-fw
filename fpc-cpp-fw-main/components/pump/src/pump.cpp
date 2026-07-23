/**
 * @file pump.cpp
 * @brief Pump class implementation.
 */

#include "pump.hpp"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

static constexpr char TAG[] = "PUMP";

namespace fpc {

// ─────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────

Pump::Pump(PumpConfig config) noexcept
    : m_config{std::move(config)}
{}

// ─────────────────────────────────────────────────────────────────────────
// init
// ─────────────────────────────────────────────────────────────────────────

Result<void> Pump::init()
{
    if (m_config.id < 0 ||
        m_config.power_hp <= 0.0f ||
        m_config.make.empty())
    {
        ESP_LOGE(TAG, "[id=%ld] init failed: invalid config", m_config.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }

    if (m_state != PumpState::NotInitialized) {
        ESP_LOGW(TAG, "[id=%ld] init: already initialised", m_config.id);
        return Result<void>::err(SystemError::InvalidState);
    }

    m_state = PumpState::Initialized;
    ESP_LOGI(TAG, "[id=%ld make=%s power=%.2f HP] initialised",
             m_config.id, m_config.make.c_str(), m_config.power_hp);
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// deinit
// ─────────────────────────────────────────────────────────────────────────

Result<void> Pump::deinit()
{
    if (m_state == PumpState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }

    m_state = PumpState::NotInitialized;
    ESP_LOGI(TAG, "[id=%ld] deinitialised", m_config.id);
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// get_state
// ─────────────────────────────────────────────────────────────────────────

PumpState Pump::get_state() const noexcept
{
    return m_state;
}

// ─────────────────────────────────────────────────────────────────────────
// set_state
// ─────────────────────────────────────────────────────────────────────────

Result<void> Pump::set_state(PumpState state)
{
    if (m_state == PumpState::NotInitialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (state == PumpState::NotInitialized || state == PumpState::Initialized) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    m_state = state;
    ESP_LOGI(TAG, "[id=%ld] state → %s", m_config.id, to_string(m_state).data());
    return Result<void>::ok();
}

// ─────────────────────────────────────────────────────────────────────────
// get_config
// ─────────────────────────────────────────────────────────────────────────

const PumpConfig& Pump::get_config() const noexcept
{
    return m_config;
}

// ─────────────────────────────────────────────────────────────────────────
// format_info
// ─────────────────────────────────────────────────────────────────────────

std::string Pump::format_info() const
{
    // Use a fixed-size stack buffer for formatting, then wrap in std::string.
    std::array<char, 256> buf{};
    std::snprintf(buf.data(), buf.size(),
        "Pump ID: %ld\n"
        " Make: %s\n"
        " Power: %.2f HP\n"
        " State: %s\n"
        " Current Rating: %.2f A\n"
        " Min Working Current: %.2f A\n",
        m_config.id,
        m_config.make.c_str(),
        m_config.power_hp,
        to_string(m_state).data(),
        m_config.current_rating,
        m_config.min_working_current);
    return std::string{buf.data()};
}

// ─────────────────────────────────────────────────────────────────────────
// format_info_into
// ─────────────────────────────────────────────────────────────────────────

Result<void> Pump::format_info_into(MutableByteView buf) const
{
    if (buf.empty()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    const std::string info = format_info();
    // info.size() + 1 for null terminator
    if (info.size() + 1u > buf.size()) {
        // Write as much as fits (null-terminated) and report overflow.
        std::memcpy(buf.data(), info.data(), buf.size() - 1u);
        buf[buf.size() - 1u] = '\0';
        return Result<void>::err(SystemError::BufferOverflow);
    }

    std::memcpy(buf.data(), info.data(), info.size() + 1u);
    return Result<void>::ok();
}

} // namespace fpc
