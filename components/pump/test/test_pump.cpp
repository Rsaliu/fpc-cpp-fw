/**
 * @file test_pump.cpp
 * @brief Unity tests for fpc::Pump.
 *
 * Test coverage:
 *   §1  Construction — stack allocation, config stored correctly.
 *   §2  init() — success, invalid configs, double-init guard.
 *   §3  deinit() — success, not-initialised guard.
 *   §4  get_state() — transitions through full lifecycle.
 *   §5  set_state() — On/Off, guards (not-init, invalid targets).
 *   §6  get_config() — values readable after construction.
 *   §7  format_info() — non-empty, contains key fields.
 *   §8  format_info_into() — success, buffer overflow detection.
 *
 * No hardware mocking needed — Pump is pure data/logic.
 */

#include "unity.h"
#include "pump.hpp"
#include <cstring>
#include <array>

using namespace fpc;

// ── Helper: default valid config ─────────────────────────────────────────
static PumpConfig make_valid_config()
{
    return PumpConfig{
        .id                  = 1,
        .make                = "Test Pump",
        .power_hp            = 5.0f,
        .current_rating      = 10.0f,
        .min_working_current = 2.0f,
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Construction
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump constructs on the stack with NotInitialized state", "[pump]")
{
    Pump pump{make_valid_config()};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::NotInitialized),
                          static_cast<int>(pump.get_state()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  init()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump::init succeeds with valid config", "[pump]")
{
    Pump pump{make_valid_config()};
    TEST_ASSERT_TRUE(pump.init().is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::Initialized),
                          static_cast<int>(pump.get_state()));
}

TEST_CASE("Pump::init returns InvalidParameter when id < 0", "[pump]")
{
    Pump pump{PumpConfig{.id = -1, .make = "Bad", .power_hp = 5.0f}};
    auto r = pump.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::init returns InvalidParameter when power_hp <= 0", "[pump]")
{
    Pump pump{PumpConfig{.id = 1, .make = "Bad", .power_hp = 0.0f}};
    auto r = pump.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::init returns InvalidParameter when make is empty", "[pump]")
{
    Pump pump{PumpConfig{.id = 1, .make = "", .power_hp = 5.0f}};
    auto r = pump.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::init returns InvalidState when called twice", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    auto r = pump.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  deinit()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump::deinit succeeds after init and returns to NotInitialized", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    TEST_ASSERT_TRUE(pump.deinit().is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::NotInitialized),
                          static_cast<int>(pump.get_state()));
}

TEST_CASE("Pump::deinit returns InvalidState when not initialised", "[pump]")
{
    Pump pump{make_valid_config()};
    auto r = pump.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::deinit succeeds from On state", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    pump.set_state(PumpState::On);
    TEST_ASSERT_TRUE(pump.deinit().is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::NotInitialized),
                          static_cast<int>(pump.get_state()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  get_state() — full lifecycle
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump state transitions: NotInitialized → Initialized → On → Off → deinit", "[pump]")
{
    Pump pump{make_valid_config()};

    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::NotInitialized),
                          static_cast<int>(pump.get_state()));

    pump.init();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::Initialized),
                          static_cast<int>(pump.get_state()));

    pump.set_state(PumpState::On);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::On),
                          static_cast<int>(pump.get_state()));

    pump.set_state(PumpState::Off);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::Off),
                          static_cast<int>(pump.get_state()));

    pump.deinit();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::NotInitialized),
                          static_cast<int>(pump.get_state()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  set_state()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump::set_state(On) succeeds when initialised", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    TEST_ASSERT_TRUE(pump.set_state(PumpState::On).is_ok());
}

TEST_CASE("Pump::set_state(Off) succeeds when initialised", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    TEST_ASSERT_TRUE(pump.set_state(PumpState::Off).is_ok());
}

TEST_CASE("Pump::set_state returns InvalidState when not initialised", "[pump]")
{
    Pump pump{make_valid_config()};
    auto r = pump.set_state(PumpState::On);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::set_state returns InvalidParameter for NotInitialized target", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    auto r = pump.set_state(PumpState::NotInitialized);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::set_state returns InvalidParameter for Initialized target", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    auto r = pump.set_state(PumpState::Initialized);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 6  get_config()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump::get_config returns correct id, make, power_hp", "[pump]")
{
    Pump pump{make_valid_config()};
    const auto& cfg = pump.get_config();

    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_STRING("Test Pump", cfg.make.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, cfg.power_hp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, cfg.current_rating);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, cfg.min_working_current);
}

// ═══════════════════════════════════════════════════════════════════════════
// § 7  format_info()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump::format_info returns non-empty string", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    auto info = pump.format_info();
    TEST_ASSERT_GREATER_THAN(0u, info.size());
}

TEST_CASE("Pump::format_info contains pump id", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    auto info = pump.format_info();
    TEST_ASSERT_NOT_NULL(std::strstr(info.c_str(), "1"));
}

TEST_CASE("Pump::format_info contains make name", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();
    auto info = pump.format_info();
    TEST_ASSERT_NOT_NULL(std::strstr(info.c_str(), "Test Pump"));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 8  format_info_into()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Pump::format_info_into succeeds with adequately-sized buffer", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();

    std::array<uint8_t, 256> buf{};
    auto r = pump.format_info_into(MutableByteView{buf});
    TEST_ASSERT_TRUE(r.is_ok());
    // Buffer must be null-terminated and non-empty.
    TEST_ASSERT_GREATER_THAN(0u, std::strlen(reinterpret_cast<const char*>(buf.data())));
}

TEST_CASE("Pump::format_info_into returns BufferOverflow for tiny buffer", "[pump]")
{
    Pump pump{make_valid_config()};
    pump.init();

    std::array<uint8_t, 10> tiny{};
    auto r = pump.format_info_into(MutableByteView{tiny});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::BufferOverflow),
                          static_cast<int>(r.error()));
}

TEST_CASE("Pump::format_info_into returns InvalidParameter for empty span", "[pump]")
{
    Pump pump{make_valid_config()};
    auto r = pump.format_info_into(MutableByteView{});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}
