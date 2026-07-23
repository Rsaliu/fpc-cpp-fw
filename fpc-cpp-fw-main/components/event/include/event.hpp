/**
 * @file event.hpp
 * @brief System-wide event types, payloads, and the MonitorEvent message struct.
 *
 * Header-only — no compilation unit needed.  All types are in namespace `fpc`.
 *
 * Design (C++17):
 *  - `enum class EventType`  replaces the reference `event_type_t` typedef enum.
 *  - `EventPayload`          is a `std::variant<>` that carries typed data
 *                            alongside the event, eliminating `void*` context pointers.
 *  - `MonitorEvent`          is the message placed on FreeRTOS queues; it bundles
 *                            EventType + EventPayload + strongly-typed IDs.
 *
 * No `void*`, no raw callbacks, no global mutable state.
 */

#pragma once

#include <cstdint>
#include <variant>
#include <string_view>
#include <optional>
#include "common.hpp"

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  EventType — strongly-typed event codes
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief System-wide event types, replacing the reference `event_type_t`.
 *
 * Each enumerator is documented with the context in which it is published:
 */
enum class EventType : uint8_t {
    TankNormal      = 0, ///< Tank level is within the normal operating range.
    TankFull        = 1, ///< Tank level has reached the full threshold.
    TankLow         = 2, ///< Tank level has dropped below the low threshold.
    PumpOvercurrent = 3, ///< Pump current exceeded the rated maximum.
    PumpNormal      = 4, ///< Pump current is within the normal range.
    PumpUndercurrent= 5, ///< Pump current fell below the minimum working level.
    Unknown         = 6, ///< Unrecognised or uninitialised event.
};

/**
 * @brief Return a human-readable name for an EventType value.
 *
 * `constexpr` and `noexcept` — evaluable at compile time, no heap allocation.
 */
[[nodiscard]] constexpr std::string_view to_string(EventType e) noexcept
{
    switch (e) {
        case EventType::TankNormal:       return "TankNormal";
        case EventType::TankFull:         return "TankFull";
        case EventType::TankLow:          return "TankLow";
        case EventType::PumpOvercurrent:  return "PumpOvercurrent";
        case EventType::PumpNormal:       return "PumpNormal";
        case EventType::PumpUndercurrent: return "PumpUndercurrent";
        case EventType::Unknown:          return "Unknown";
        default:                          return "UnknownEvent";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  EventPayload — typed data carried alongside an event
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Payload for a tank-related event.
 *
 * Carries the raw ADC level reading at the time the event was raised.
 */
struct TankEventPayload {
    uint16_t level_raw; ///< Raw level sensor reading (e.g. ADC counts).
};

/**
 * @brief Payload for a pump-related event.
 *
 * Carries the measured current (Amps) at the time the event was raised.
 */
struct PumpEventPayload {
    float current_amps; ///< Measured phase current in Amperes.
};

/**
 * @brief Variant-based event payload.
 *
 * - `std::monostate`  → no data (e.g. EventType::Unknown).
 * - `TankEventPayload`→ level reading for tank events.
 * - `PumpEventPayload`→ current reading for pump events.
 *
 * Usage:
 * @code
 *   EventPayload p = TankEventPayload{.level_raw = 512u};
 *   if (auto* tp = std::get_if<TankEventPayload>(&p)) {
 *       ESP_LOGI(TAG, "level=%u", tp->level_raw);
 *   }
 * @endcode
 */
using EventPayload = std::variant<
    std::monostate,    ///< No payload (sentinel / unknown).
    TankEventPayload,  ///< Tank-related payload.
    PumpEventPayload   ///< Pump-related payload.
>;

// ═══════════════════════════════════════════════════════════════════════════
// § 3  MonitorEvent — the queue message
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief A self-contained event message placed on FreeRTOS queues.
 *
 * Replaces the reference `monitor_event_queue_t` struct:
 *  - `void* context`  → eliminated; use the typed `payload` field instead.
 *  - `int actuator_id`→ `int32_t actuator_id` (explicit width).
 *  - `int monitor_id` → `int32_t monitor_id`.
 *  - `event_type_t`   → `EventType` (enum class).
 *
 * The struct is trivially copyable (all members are value types) so it can
 * be safely sent through a FreeRTOS queue with `xQueueSend`.
 *
 * Note: `std::variant` with non-trivial alternatives (like `PumpEventPayload`
 * which contains `float`) is not trivially copyable in general, but it IS
 * copy/move-constructible and copy/move-assignable, which is sufficient for
 * FreeRTOS `xQueueSend`/`xQueueReceive` (both do a byte-level `memcpy` of the
 * item size).  Keep item size <= ~64 bytes; current size: ~12 bytes on Xtensa.
 */
struct MonitorEvent {
    EventType   type{EventType::Unknown};  ///< What happened.
    EventPayload payload{std::monostate{}}; ///< Typed data attached to the event.
    int32_t     monitor_id{-1};            ///< ID of the monitor that published the event.
    int32_t     actuator_id{-1};           ///< ID of the actuator this event targets (-1 = broadcast).

    // ── Convenience factory methods ──────────────────────────────────────

    /// Build a tank event with a level reading.
    [[nodiscard]] static MonitorEvent tank(EventType t, int32_t mon_id,
                                           uint16_t level_raw,
                                           int32_t act_id = -1) noexcept
    {
        return MonitorEvent{t, TankEventPayload{level_raw}, mon_id, act_id};
    }

    /// Build a pump event with a current reading.
    [[nodiscard]] static MonitorEvent pump(EventType t, int32_t mon_id,
                                           float current_amps,
                                           int32_t act_id = -1) noexcept
    {
        return MonitorEvent{t, PumpEventPayload{current_amps}, mon_id, act_id};
    }

    /// Build an unknown / no-payload event.
    [[nodiscard]] static MonitorEvent unknown(int32_t mon_id = -1) noexcept
    {
        return MonitorEvent{EventType::Unknown, std::monostate{}, mon_id, -1};
    }
};

} // namespace fpc
