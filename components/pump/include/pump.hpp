/**
 * @file pump.hpp
 * @brief Pump data model — C++17 state-machine class.
 *
 * The `Pump` class represents the logical pump entity (configuration +
 * state).  It does NOT drive GPIO — relay control is handled by
 * `relay_driver`.  Pump monitors observe this object and drive relays in
 * response to sensor readings.
 *
 * C++17 features used:
 *  - `enum class PumpState`     — replaces reference `pump_state_t`.
 *  - `std::string` for `make`   — replaces raw `char*`.
 *  - `Result<T>` return values  — replaces `error_type_t` out-parameters.
 *  - `std::string_view`         — zero-allocation read-only string parameters.
 *  - Stack-allocated object     — no `new`/`malloc`; callers own via
 *                                  `std::unique_ptr` or stack.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include "common.hpp"

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  PumpState
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Pump lifecycle state.
 *
 * Transition diagram:
 *
 *   NotInitialized ──init()──► Initialized ──(monitor sets)──► On
 *                                    ▲                          │
 *                                    └──────────────────────────┘
 *                              deinit() returns to NotInitialized from any state.
 */
enum class PumpState : uint8_t {
    NotInitialized = 0, ///< Object created but init() not yet called.
    Initialized    = 1, ///< init() succeeded; motor at rest, ready for control.
    On             = 2, ///< Pump running (set by pump monitor / control unit).
    Off            = 3, ///< Pump stopped (set by pump monitor / control unit).
};

[[nodiscard]] constexpr std::string_view to_string(PumpState s) noexcept
{
    switch (s) {
        case PumpState::NotInitialized: return "NotInitialized";
        case PumpState::Initialized:    return "Initialized";
        case PumpState::On:             return "On";
        case PumpState::Off:            return "Off";
        default:                        return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  PumpConfig
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Immutable pump specification set at construction time.
 *
 * `make` is a `std::string` (owned) — no raw `char*` in the public API.
 */
struct PumpConfig {
    int32_t     id{-1};                ///< Application-level identifier (>= 0).
    std::string make{};                ///< Manufacturer / model name.
    float       power_hp{0.0f};        ///< Rated power in horse-power (> 0).
    float       current_rating{0.0f};  ///< Rated full-load current (Amps).
    float       min_working_current{0.0f}; ///< Minimum expected running current.
};

// ═══════════════════════════════════════════════════════════════════════════
// § 3  Pump
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Pump data-model and lifecycle state machine.
 *
 * Owns its `PumpConfig` by value.  The class is non-copyable (a pump is a
 * unique physical device) but movable.
 *
 * Usage:
 * @code
 *   Pump pump{PumpConfig{.id=1, .make="Grundfos", .power_hp=5.0f,
 *                        .current_rating=10.0f, .min_working_current=2.0f}};
 *   pump.init();
 *   pump.set_state(PumpState::On);
 *   ESP_LOGI(TAG, "%s", pump.format_info().c_str());
 * @endcode
 */
class Pump {
public:
    /**
     * @param config  Pump specification (moved in).
     */
    explicit Pump(PumpConfig config) noexcept;

    // Non-copyable — represents a unique physical device.
    Pump(const Pump&)            = delete;
    Pump& operator=(const Pump&) = delete;

    // Movable.
    Pump(Pump&&)            = default;
    Pump& operator=(Pump&&) = default;

    ~Pump() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────

    /**
     * @brief Validate config and transition to `Initialized`.
     *
     * Fails with:
     *  - `InvalidParameter`  if id < 0, power_hp <= 0, or make is empty.
     *  - `InvalidState`      if already initialised.
     */
    Result<void> init();

    /**
     * @brief Release resources and return to `NotInitialized`.
     *
     * Fails with `InvalidState` if not currently initialised.
     */
    Result<void> deinit();

    // ── State ─────────────────────────────────────────────────────────────

    /**
     * @brief Read the current lifecycle state.
     * @return `PumpState` value (never fails).
     */
    [[nodiscard]] PumpState get_state() const noexcept;

    /**
     * @brief Set the runtime state (On/Off) — called by pump monitor.
     *
     * Only `PumpState::On` and `PumpState::Off` are valid targets.
     * Fails with `InvalidState` if not initialised, or `InvalidParameter`
     * if @p state is `NotInitialized` / `Initialized`.
     */
    Result<void> set_state(PumpState state);

    // ── Config access ──────────────────────────────────────────────────────

    /// Return a const reference to the pump configuration.
    [[nodiscard]] const PumpConfig& get_config() const noexcept;

    // ── Diagnostics ────────────────────────────────────────────────────────

    /**
     * @brief Format pump information into a heap-allocated string.
     *
     * Replaces reference `pump_print_info_into_buffer` — returns a proper
     * `std::string` so the caller never needs to manage buffer sizes.
     */
    [[nodiscard]] std::string format_info() const;

    /**
     * @brief Write a formatted info string into a caller-provided buffer.
     *
     * Provided for compatibility with fixed-stack callers.  The output is
     * null-terminated.  Returns `BufferOverflow` if @p buf is too small.
     *
     * @param buf   Destination buffer (mutable byte span).
     * @return      `Ok` on success, `BufferOverflow` if truncated.
     */
    Result<void> format_info_into(MutableByteView buf) const;

private:
    PumpConfig m_config;
    PumpState  m_state{PumpState::NotInitialized};
};

} // namespace fpc
