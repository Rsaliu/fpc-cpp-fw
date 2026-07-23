/**
 * @file relay_driver.cpp
 * @brief Concrete relay driver implementation — EspGpioDriver + Relay.
 */

#include "relay_driver.hpp"
#include "esp_log.h"

static constexpr char TAG[] = "RELAY";

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// EspGpioDriver
// ═══════════════════════════════════════════════════════════════════════════

esp_err_t EspGpioDriver::reset_pin(gpio_num_t pin)
{
    return gpio_reset_pin(pin) == ESP_OK ? ESP_OK : ESP_FAIL;
}

esp_err_t EspGpioDriver::set_direction(gpio_num_t pin, gpio_mode_t mode)
{
    return gpio_set_direction(pin, mode);
}

esp_err_t EspGpioDriver::set_level(gpio_num_t pin, uint32_t level)
{
    return gpio_set_level(pin, level);
}

// ═══════════════════════════════════════════════════════════════════════════
// Relay
// ═══════════════════════════════════════════════════════════════════════════

Relay::Relay(RelayConfig config, IGpioDriver& gpio) noexcept
    : m_config{config}, m_gpio{gpio}
{}

Relay::~Relay()
{
    if (m_initialized) {
        // Drive LOW and mark uninitialised — ignore errors in destructor.
        m_gpio.set_level(m_config.pin, 0u);
        m_initialized = false;
        m_state = RelayState::Off;
    }
}

// ─── init ────────────────────────────────────────────────────────────────

Result<void> Relay::init()
{
    if (m_config.id < 0) {
        ESP_LOGE(TAG, "[id=%ld] init failed: invalid id", m_config.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }

    if (m_gpio.reset_pin(m_config.pin) != ESP_OK) {
        return Result<void>::err(SystemError::Failed);
    }
    if (m_gpio.set_direction(m_config.pin, GPIO_MODE_OUTPUT) != ESP_OK) {
        return Result<void>::err(SystemError::Failed);
    }
    if (m_gpio.set_level(m_config.pin, 0u) != ESP_OK) {
        return Result<void>::err(SystemError::Failed);
    }

    m_initialized = true;
    m_state = RelayState::Off;
    ESP_LOGI(TAG, "[id=%ld pin=%d] initialised", m_config.id, m_config.pin);
    return Result<void>::ok();
}

// ─── deinit ──────────────────────────────────────────────────────────────

Result<void> Relay::deinit()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_gpio.set_level(m_config.pin, 0u);
    m_initialized = false;
    m_state = RelayState::Off;
    ESP_LOGI(TAG, "[id=%ld] deinitialised", m_config.id);
    return Result<void>::ok();
}

// ─── on ──────────────────────────────────────────────────────────────────

Result<void> Relay::on()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (m_state == RelayState::Tripped) {
        ESP_LOGW(TAG, "[id=%ld] on() refused — relay is tripped", m_config.id);
        return Result<void>::err(SystemError::InvalidState);
    }
    m_gpio.set_level(m_config.pin, 1u);
    m_state = RelayState::On;
    return Result<void>::ok();
}

// ─── off ─────────────────────────────────────────────────────────────────

Result<void> Relay::off()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (m_state == RelayState::Tripped) {
        ESP_LOGW(TAG, "[id=%ld] off() refused — relay is tripped", m_config.id);
        return Result<void>::err(SystemError::InvalidState);
    }
    m_gpio.set_level(m_config.pin, 0u);
    m_state = RelayState::Off;
    return Result<void>::ok();
}

// ─── trip ────────────────────────────────────────────────────────────────

Result<void> Relay::trip()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    if (m_state == RelayState::Tripped) {
        ESP_LOGW(TAG, "[id=%ld] trip(): already tripped — no-op", m_config.id);
        return Result<void>::ok();  // idempotent
    }
    m_gpio.set_level(m_config.pin, 0u);
    m_state = RelayState::Tripped;
    ESP_LOGW(TAG, "[id=%ld] TRIPPED", m_config.id);
    return Result<void>::ok();
}

// ─── reset ───────────────────────────────────────────────────────────────

Result<void> Relay::reset()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_gpio.set_level(m_config.pin, 0u);
    m_state = RelayState::Off;
    return Result<void>::ok();
}

// ─── reset_and_on ────────────────────────────────────────────────────────

Result<void> Relay::reset_and_on()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_gpio.set_level(m_config.pin, 1u);
    m_state = RelayState::On;
    return Result<void>::ok();
}

// ─── get_state ───────────────────────────────────────────────────────────

Result<RelayState> Relay::get_state() const
{
    if (!m_initialized) {
        return Result<RelayState>::err(SystemError::InvalidState);
    }
    return Result<RelayState>::ok(m_state);
}

// ─── get_config ──────────────────────────────────────────────────────────

RelayConfig Relay::get_config() const
{
    return m_config;
}

} // namespace fpc
