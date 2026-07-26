/**
 * @file tank.hpp
 * @brief Tank data model — C++17 state-machine class with typed level representation.
 *
 * C++17 features used:
 *  - `enum class TankShape` / `TankState`  — replaces C typedef enums.
 *  - `constexpr` free functions             — `to_string()`, `shape_from_string()`.
 *  - `TankConfig` with value types          — no raw pointers, no `char*`.
 *  - `Tank` class with RAII lifecycle       — non-copyable, movable.
 *  - `Result<T>` return values              — replaces `error_type_t` out-params.
 *  - `MutableByteView` / `std::string`      — for format_info_into / format_info.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <optional>
#include "common.hpp"

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  TankShape
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Physical shape of the tank — determines volume calculation method.
 */
enum class TankShape : uint8_t {
    Rectangle = 0, ///< Rectangular / box-shaped tank.
    Cylinder  = 1, ///< Cylindrical tank.
};

[[nodiscard]] constexpr std::string_view to_string(TankShape s) noexcept
{
    switch (s) {
        case TankShape::Rectangle: return "Rectangle";
        case TankShape::Cylinder:  return "Cylinder";
        default:                   return "Unknown";
    }
}

/**
 * @brief Parse a shape name string into a `TankShape`.
 *
 * @param s  Case-sensitive shape name ("Rectangle" or "Cylinder").
 * @return   The matching `TankShape`, or `std::nullopt` if unrecognised.
 */
[[nodiscard]] inline std::optional<TankShape> shape_from_string(std::string_view s) noexcept
{
    if (s == "Rectangle") return TankShape::Rectangle;
    if (s == "Cylinder")  return TankShape::Cylinder;
    return std::nullopt;
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  TankState
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Tank lifecycle state.
 */
enum class TankState : uint8_t {
    NotInitialized = 0, ///< Created but init() not yet called.
    Initialized    = 1, ///< Ready for use.
};

[[nodiscard]] constexpr std::string_view to_string(TankState s) noexcept
{
    switch (s) {
        case TankState::NotInitialized: return "NotInitialized";
        case TankState::Initialized:    return "Initialized";
        default:                        return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  TankConfig
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Immutable tank specification, set at construction time.
 *
 * Level thresholds are in millimetres to match the RS485 sensor output.
 */
struct TankConfig {
    int32_t   id{-1};                       ///< Application-level identifier (>= 0).
    float     capacity_litres{0.0f};        ///< Total volume in litres (> 0).
    TankShape shape{TankShape::Rectangle};  ///< Physical shape.
    float     height_mm{0.0f};             ///< Physical height in centimetres (> 0).
    int32_t   full_level_mm{0};            ///< Sensor reading that means "full" (mm).
    int32_t   low_level_mm{0};             ///< Sensor reading that means "low" (mm).
};

// ═══════════════════════════════════════════════════════════════════════════
// § 4  Tank
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Tank data-model and lifecycle state machine.
 *
 * Owns its `TankConfig` by value — no heap allocation inside the class.
 * Non-copyable (represents a unique physical vessel); movable.
 *
 * Usage:
 * @code
 *   Tank tank{TankConfig{.id=1, .capacity_litres=1000.f,
 *                        .shape=TankShape::Rectangle,
 *                        .height_mm=100.f*10,
 *                        .full_level_mm=900, .low_level_mm=100}};
 *   tank.init();
 *   ESP_LOGI(TAG, "%s", tank.format_info().c_str());
 * @endcode
 */
class Tank {
public:
    explicit Tank(TankConfig config) noexcept;

    Tank(const Tank&)            = delete;
    Tank& operator=(const Tank&) = delete;
    Tank(Tank&&)                 = default;
    Tank& operator=(Tank&&)      = default;
    ~Tank()                      = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────

    /**
     * @brief Validate configuration and move to `Initialized`.
     *
     * Fails with:
     *  - `InvalidParameter` if id < 0, capacity <= 0, height <= 0,
     *                        full_level_mm <= low_level_mm, or either < 0.
     *  - `InvalidState`     if already initialised.
     */
    Result<void> init();

    /**
     * @brief Return to `NotInitialized`.
     *
     * Fails with `InvalidState` if not currently initialised.
     */
    Result<void> deinit();

    // ── Accessors ─────────────────────────────────────────────────────────

    /// Current lifecycle state (never fails).
    [[nodiscard]] TankState get_state()  const noexcept;

    /// Immutable reference to configuration.
    [[nodiscard]] const TankConfig& get_config() const noexcept;

    // ── Diagnostics ───────────────────────────────────────────────────────

    /// Return a formatted info string (heap-allocated).
    [[nodiscard]] std::string format_info() const;

    /**
     * @brief Write formatted info into caller-supplied buffer.
     *
     * @return `Ok` on success, `BufferOverflow` if the buffer is too small,
     *         `InvalidParameter` if the span is empty.
     */
    Result<void> format_info_into(MutableByteView buf) const;

private:
    TankConfig m_config;
    TankState  m_state{TankState::NotInitialized};
};

} // namespace fpc
