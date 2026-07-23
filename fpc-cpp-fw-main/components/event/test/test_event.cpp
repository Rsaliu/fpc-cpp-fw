/**
 * @file test_event.cpp
 * @brief Unity tests for fpc::event (header-only component).
 *
 * Test coverage:
 *   §1  EventType to_string()  — all enumerators + out-of-range cast.
 *   §2  EventPayload variant   — construction, std::get_if, std::holds_alternative.
 *   §3  MonitorEvent           — default construction, factory methods,
 *                                field values, copy semantics.
 *   §4  Boundary / edge cases  — unknown event factory, broadcast actuator_id.
 *
 * No hardware mock needed — all logic is pure C++ types.
 */

#include "unity.h"
#include "event.hpp"
#include <variant>
#include <cstdint>

using namespace fpc;

// ═══════════════════════════════════════════════════════════════════════════
// § 1  EventType to_string()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("to_string(EventType::TankNormal) == 'TankNormal'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("TankNormal", to_string(EventType::TankNormal).data());
}

TEST_CASE("to_string(EventType::TankFull) == 'TankFull'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("TankFull", to_string(EventType::TankFull).data());
}

TEST_CASE("to_string(EventType::TankLow) == 'TankLow'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("TankLow", to_string(EventType::TankLow).data());
}

TEST_CASE("to_string(EventType::PumpOvercurrent) == 'PumpOvercurrent'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("PumpOvercurrent",
                             to_string(EventType::PumpOvercurrent).data());
}

TEST_CASE("to_string(EventType::PumpNormal) == 'PumpNormal'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("PumpNormal", to_string(EventType::PumpNormal).data());
}

TEST_CASE("to_string(EventType::PumpUndercurrent) == 'PumpUndercurrent'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("PumpUndercurrent",
                             to_string(EventType::PumpUndercurrent).data());
}

TEST_CASE("to_string(EventType::Unknown) == 'Unknown'", "[event]")
{
    TEST_ASSERT_EQUAL_STRING("Unknown", to_string(EventType::Unknown).data());
}

TEST_CASE("to_string returns 'UnknownEvent' for out-of-range cast", "[event]")
{
    auto bad = static_cast<EventType>(0xFFu);
    TEST_ASSERT_EQUAL_STRING("UnknownEvent", to_string(bad).data());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  EventPayload variant
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("EventPayload default-constructs to std::monostate", "[event]")
{
    EventPayload p{};
    TEST_ASSERT_TRUE(std::holds_alternative<std::monostate>(p));
}

TEST_CASE("EventPayload holds TankEventPayload and level_raw is correct", "[event]")
{
    EventPayload p = TankEventPayload{.level_raw = 1024u};
    TEST_ASSERT_TRUE(std::holds_alternative<TankEventPayload>(p));

    const auto* tp = std::get_if<TankEventPayload>(&p);
    TEST_ASSERT_NOT_NULL(tp);
    TEST_ASSERT_EQUAL_UINT16(1024u, tp->level_raw);
}

TEST_CASE("EventPayload holds PumpEventPayload and current_amps is correct", "[event]")
{
    EventPayload p = PumpEventPayload{.current_amps = 3.5f};
    TEST_ASSERT_TRUE(std::holds_alternative<PumpEventPayload>(p));

    const auto* pp = std::get_if<PumpEventPayload>(&p);
    TEST_ASSERT_NOT_NULL(pp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, pp->current_amps);
}

TEST_CASE("std::get_if returns nullptr for wrong alternative", "[event]")
{
    EventPayload p = TankEventPayload{.level_raw = 512u};
    // PumpEventPayload is not held — get_if should return nullptr.
    TEST_ASSERT_NULL(std::get_if<PumpEventPayload>(&p));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  MonitorEvent construction and factory methods
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("MonitorEvent default construction has Unknown type and id -1", "[event]")
{
    MonitorEvent ev{};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(EventType::Unknown),
                          static_cast<int>(ev.type));
    TEST_ASSERT_EQUAL_INT(-1, ev.monitor_id);
    TEST_ASSERT_EQUAL_INT(-1, ev.actuator_id);
    TEST_ASSERT_TRUE(std::holds_alternative<std::monostate>(ev.payload));
}

TEST_CASE("MonitorEvent::tank factory sets correct fields", "[event]")
{
    auto ev = MonitorEvent::tank(EventType::TankFull, /*mon_id=*/2, /*level=*/800u);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(EventType::TankFull),
                          static_cast<int>(ev.type));
    TEST_ASSERT_EQUAL_INT(2, ev.monitor_id);
    TEST_ASSERT_EQUAL_INT(-1, ev.actuator_id);  // broadcast default

    const auto* tp = std::get_if<TankEventPayload>(&ev.payload);
    TEST_ASSERT_NOT_NULL(tp);
    TEST_ASSERT_EQUAL_UINT16(800u, tp->level_raw);
}

TEST_CASE("MonitorEvent::tank factory with explicit actuator_id", "[event]")
{
    auto ev = MonitorEvent::tank(EventType::TankLow, 1, 200u, /*act_id=*/5);
    TEST_ASSERT_EQUAL_INT(5, ev.actuator_id);
}

TEST_CASE("MonitorEvent::pump factory sets correct fields", "[event]")
{
    auto ev = MonitorEvent::pump(EventType::PumpOvercurrent, /*mon_id=*/3,
                                 /*current=*/8.2f);

    TEST_ASSERT_EQUAL_INT(static_cast<int>(EventType::PumpOvercurrent),
                          static_cast<int>(ev.type));
    TEST_ASSERT_EQUAL_INT(3, ev.monitor_id);

    const auto* pp = std::get_if<PumpEventPayload>(&ev.payload);
    TEST_ASSERT_NOT_NULL(pp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.2f, pp->current_amps);
}

TEST_CASE("MonitorEvent::unknown factory sets type to Unknown", "[event]")
{
    auto ev = MonitorEvent::unknown(7);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(EventType::Unknown),
                          static_cast<int>(ev.type));
    TEST_ASSERT_EQUAL_INT(7, ev.monitor_id);
    TEST_ASSERT_TRUE(std::holds_alternative<std::monostate>(ev.payload));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  Copy semantics — MonitorEvent is value-copyable (needed for queues)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("MonitorEvent copy produces independent value-equal copy", "[event]")
{
    auto original = MonitorEvent::pump(EventType::PumpNormal, 1, 4.0f, 2);
    MonitorEvent copy = original;  // copy-construct

    TEST_ASSERT_EQUAL_INT(static_cast<int>(original.type),
                          static_cast<int>(copy.type));
    TEST_ASSERT_EQUAL_INT(original.monitor_id, copy.monitor_id);
    TEST_ASSERT_EQUAL_INT(original.actuator_id, copy.actuator_id);

    const auto* pp = std::get_if<PumpEventPayload>(&copy.payload);
    TEST_ASSERT_NOT_NULL(pp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, pp->current_amps);
}
