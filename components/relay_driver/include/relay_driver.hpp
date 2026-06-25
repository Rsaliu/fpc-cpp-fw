/**
 * @file relay_driver.hpp
 * @brief RAII GPIO relay driver — C++17 interface + concrete implementation.
 *
 * Architecture (dependency injection pattern):
 *
 *   IGpioDriver  ← abstraction over ESP-IDF gpio_* calls
 *       └─ EspGpioDriver   (production: calls real ESP-IDF gpio functions)
 *       └─ MockGpioDriver  (tests: records calls, returns configurable codes)
 *
 *   IRelay       ← public relay abstraction (interface for mocking by consumers)
 *       └─ Relay           (concrete: owns a RelayConfig, holds IGpioDriver ref)
 *
 * All return values use `fpc::Result<void>` / `fpc::Result<RelayState>`.
 * No raw `new`/`delete` — callers manage lifetime via `std::unique_ptr`.
 */

#pragma once

#include <cstdint>
#include <string_view>
#include "driver/gpio.h"
#include "common.hpp"

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  RelayState
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Relay operating state.
 */
enum class RelayState : uint8_t {
    Off     = 0, ///< De-energised (GPIO LOW).
    On      = 1, ///< Energised (GPIO HIGH).
    Tripped = 2, ///< Fault — de-energised and locked out until reset.
};

[[nodiscard]] constexpr std::string_view to_string(RelayState s) noexcept
{
    switch (s) {
        case RelayState::Off:     return "Off";
        case RelayState::On:      return "On";
        case RelayState::Tripped: return "Tripped";
        default:                  return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  RelayConfig
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Immutable relay configuration (set at construction time).
 */
struct RelayConfig {
    int32_t     id;  ///< Application-level relay identifier (must be >= 0).
    gpio_num_t  pin; ///< GPIO pin number driving the relay coil.
};

// ═══════════════════════════════════════════════════════════════════════════
// § 3  IGpioDriver — injectable GPIO abstraction
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Thin abstraction over ESP-IDF GPIO operations.
 *
 * Production code uses `EspGpioDriver` (below).
 * Tests inject a `MockGpioDriver` to avoid real GPIO hardware.
 */
class IGpioDriver {
public:
    virtual ~IGpioDriver() = default;

    virtual esp_err_t reset_pin(gpio_num_t pin)                                 = 0;
    virtual esp_err_t set_direction(gpio_num_t pin, gpio_mode_t mode)           = 0;
    virtual esp_err_t set_level(gpio_num_t pin, uint32_t level)                 = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 4  EspGpioDriver — production implementation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Production GPIO driver — delegates directly to ESP-IDF gpio_* APIs.
 *
 * Instantiate once and pass by reference to all `Relay` objects.
 */
class EspGpioDriver final : public IGpioDriver {
public:
    esp_err_t reset_pin(gpio_num_t pin) override;
    esp_err_t set_direction(gpio_num_t pin, gpio_mode_t mode) override;
    esp_err_t set_level(gpio_num_t pin, uint32_t level) override;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 5  IRelay — relay abstraction for consumers
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Pure interface for a relay actuator.
 *
 * Consumers (e.g. `pump_control_unit`) depend only on this interface,
 * making them testable without real hardware.
 */
class IRelay {
public:
    virtual ~IRelay() = default;

    /// Initialise the GPIO pin — must be called before any other operation.
    virtual Result<void>       init()          = 0;

    /// Release the GPIO pin and leave the relay in Off state.
    virtual Result<void>       deinit()        = 0;

    /// Energise the relay. Fails if not initialised or in Tripped state.
    virtual Result<void>       on()            = 0;

    /// De-energise the relay. Fails if not initialised or in Tripped state.
    virtual Result<void>       off()           = 0;

    /**
     * @brief Enter fault (Tripped) state — de-energises and locks out.
     *
     * Calling trip() on an already-tripped relay is a no-op (returns Ok).
     */
    virtual Result<void>       trip()          = 0;

    /// Clear fault state → relay moves to Off. Fails if not initialised.
    virtual Result<void>       reset()         = 0;

    /// Reset fault state and immediately energise. Fails if not initialised.
    virtual Result<void>       reset_and_on()  = 0;

    /// Return current state. Fails if not initialised.
    virtual Result<RelayState> get_state()     const = 0;

    /// Return immutable configuration.
    virtual RelayConfig        get_config()    const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 6  Relay — concrete RAII implementation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Concrete relay implementation.
 *
 * Owns a `RelayConfig` (by value) and holds a non-owning reference to an
 * `IGpioDriver`.  The GPIO driver's lifetime must exceed that of this object.
 *
 * Usage:
 * @code
 *   EspGpioDriver gpio;
 *   Relay relay{RelayConfig{.id=1, .pin=GPIO_NUM_5}, gpio};
 *   relay.init();
 *   relay.on();
 *   relay.off();
 *   relay.deinit();   // or just let it destruct (RAII)
 * @endcode
 */
class Relay final : public IRelay {
public:
    /**
     * @param config  Relay configuration (copied).
     * @param gpio    Reference to a GPIO driver (must outlive this object).
     */
    Relay(RelayConfig config, IGpioDriver& gpio) noexcept;

    /**
     * @brief Destructor — calls `deinit()` automatically if still initialised
     *        (RAII: ensures the GPIO pin is driven LOW on teardown).
     */
    ~Relay() override;

    // Non-copyable — owns a GPIO resource.
    Relay(const Relay&)            = delete;
    Relay& operator=(const Relay&) = delete;

    // Movable.
    Relay(Relay&&)            = default;
    Relay& operator=(Relay&&) = default;

    Result<void>       init()         override;
    Result<void>       deinit()       override;
    Result<void>       on()           override;
    Result<void>       off()          override;
    Result<void>       trip()         override;
    Result<void>       reset()        override;
    Result<void>       reset_and_on() override;
    Result<RelayState> get_state()    const override;
    RelayConfig        get_config()   const override;

private:
    RelayConfig   m_config;
    IGpioDriver&  m_gpio;
    bool          m_initialized{false};
    RelayState    m_state{RelayState::Off};
};

} // namespace fpc
