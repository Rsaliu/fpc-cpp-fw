/**
 * @file test_tank.cpp
 * @brief Unity tests for fpc::Tank.
 *
 * Test coverage:
 *   §1  TankShape helpers — to_string, shape_from_string (valid + invalid).
 *   §2  Construction — stack allocation, state starts NotInitialized.
 *   §3  init() — success, every invalid-config branch, double-init guard.
 *   §4  deinit() — success, not-initialised guard.
 *   §5  get_state() — transitions.
 *   §6  get_config() — all fields readable.
 *   §7  format_info() — non-empty, contains key fields.
 *   §8  format_info_into() — success, overflow, empty-span guard.
 *
 * No hardware mocking needed — Tank is pure data/logic.
 */

#include "unity.h"
#include "tank.hpp"
#include <cstring>
#include <array>

using namespace fpc;

// ── Helper: default valid config ─────────────────────────────────────────
static TankConfig make_valid_config()
{
    return TankConfig{
        .id              = 1,
        .capacity_litres = 1000.0f,
        .shape           = TankShape::Rectangle,
        .height_mm       = 100.0f * 10,
        .full_level_mm   = 900,
        .low_level_mm    = 100,
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// § 1  TankShape helpers
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("to_string(TankShape::Rectangle) == 'Rectangle'", "[tank]")
{
    TEST_ASSERT_EQUAL_STRING("Rectangle", to_string(TankShape::Rectangle).data());
}

TEST_CASE("to_string(TankShape::Cylinder) == 'Cylinder'", "[tank]")
{
    TEST_ASSERT_EQUAL_STRING("Cylinder", to_string(TankShape::Cylinder).data());
}

TEST_CASE("shape_from_string parses 'Rectangle' correctly", "[tank]")
{
    auto s = shape_from_string("Rectangle");
    TEST_ASSERT_TRUE(s.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankShape::Rectangle),
                          static_cast<int>(s.value()));
}

TEST_CASE("shape_from_string parses 'Cylinder' correctly", "[tank]")
{
    auto s = shape_from_string("Cylinder");
    TEST_ASSERT_TRUE(s.has_value());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankShape::Cylinder),
                          static_cast<int>(s.value()));
}

TEST_CASE("shape_from_string returns nullopt for unknown string", "[tank]")
{
    TEST_ASSERT_FALSE(shape_from_string("Oval").has_value());
}

TEST_CASE("shape_from_string returns nullopt for empty string", "[tank]")
{
    TEST_ASSERT_FALSE(shape_from_string("").has_value());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Construction
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank constructs with NotInitialized state", "[tank]")
{
    Tank tank{make_valid_config()};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::NotInitialized),
                          static_cast<int>(tank.get_state()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  init()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank::init succeeds with valid config", "[tank]")
{
    Tank tank{make_valid_config()};
    TEST_ASSERT_TRUE(tank.init().is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::Initialized),
                          static_cast<int>(tank.get_state()));
}

TEST_CASE("Tank::init returns InvalidParameter when id < 0", "[tank]")
{
    Tank tank{TankConfig{.id=-1, .capacity_litres=1000.f,
                         .shape=TankShape::Rectangle, .height_mm=100.f*10,
                         .full_level_mm=900, .low_level_mm=100}};
    auto r = tank.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Tank::init returns InvalidParameter when capacity <= 0", "[tank]")
{
    Tank tank{TankConfig{.id=1, .capacity_litres=0.f,
                         .shape=TankShape::Rectangle, .height_mm=100.f*10,
                         .full_level_mm=900, .low_level_mm=100}};
    TEST_ASSERT_TRUE(tank.init().is_err());
}

TEST_CASE("Tank::init returns InvalidParameter when height <= 0", "[tank]")
{
    Tank tank{TankConfig{.id=1, .capacity_litres=1000.f,
                         .shape=TankShape::Rectangle, .height_mm=0.f,
                         .full_level_mm=900, .low_level_mm=100}};
    TEST_ASSERT_TRUE(tank.init().is_err());
}

TEST_CASE("Tank::init returns InvalidParameter when full_level <= low_level", "[tank]")
{
    Tank tank{TankConfig{.id=1, .capacity_litres=1000.f,
                         .shape=TankShape::Rectangle, .height_mm=100.f*10,
                         .full_level_mm=100, .low_level_mm=900}}; // swapped
    TEST_ASSERT_TRUE(tank.init().is_err());
}

TEST_CASE("Tank::init returns InvalidParameter when levels are equal", "[tank]")
{
    Tank tank{TankConfig{.id=1, .capacity_litres=1000.f,
                         .shape=TankShape::Rectangle, .height_mm=100.f*10,
                         .full_level_mm=500, .low_level_mm=500}};
    TEST_ASSERT_TRUE(tank.init().is_err());
}

TEST_CASE("Tank::init returns InvalidState when called twice", "[tank]")
{
    Tank tank{make_valid_config()};
    tank.init();
    auto r = tank.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("Tank::init works for Cylinder shape", "[tank]")
{
    Tank tank{TankConfig{.id=2, .capacity_litres=500.f,
                         .shape=TankShape::Cylinder, .height_mm=80.f*10,
                         .full_level_mm=750, .low_level_mm=50}};
    TEST_ASSERT_TRUE(tank.init().is_ok());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  deinit()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank::deinit succeeds after init and returns to NotInitialized", "[tank]")
{
    Tank tank{make_valid_config()};
    tank.init();
    TEST_ASSERT_TRUE(tank.deinit().is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::NotInitialized),
                          static_cast<int>(tank.get_state()));
}

TEST_CASE("Tank::deinit returns InvalidState when not initialised", "[tank]")
{
    Tank tank{make_valid_config()};
    auto r = tank.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  get_state() transitions
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank state: NotInitialized → Initialized → NotInitialized", "[tank]")
{
    Tank tank{make_valid_config()};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::NotInitialized),
                          static_cast<int>(tank.get_state()));
    tank.init();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::Initialized),
                          static_cast<int>(tank.get_state()));
    tank.deinit();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::NotInitialized),
                          static_cast<int>(tank.get_state()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 6  get_config()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank::get_config returns all fields correctly", "[tank]")
{
    Tank tank{make_valid_config()};
    const auto& cfg = tank.get_config();

    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, cfg.capacity_litres);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankShape::Rectangle),
                          static_cast<int>(cfg.shape));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, cfg.height_mm);
    TEST_ASSERT_EQUAL_INT(900, cfg.full_level_mm);
    TEST_ASSERT_EQUAL_INT(100, cfg.low_level_mm);
}

// ═══════════════════════════════════════════════════════════════════════════
// § 7  format_info()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank::format_info returns non-empty string", "[tank]")
{
    Tank tank{make_valid_config()};
    tank.init();
    TEST_ASSERT_GREATER_THAN(0u, tank.format_info().size());
}

TEST_CASE("Tank::format_info contains tank id", "[tank]")
{
    Tank tank{make_valid_config()};
    auto info = tank.format_info();
    TEST_ASSERT_NOT_NULL(std::strstr(info.c_str(), "1"));
}

TEST_CASE("Tank::format_info contains shape name", "[tank]")
{
    Tank tank{make_valid_config()};
    auto info = tank.format_info();
    TEST_ASSERT_NOT_NULL(std::strstr(info.c_str(), "Rectangle"));
}

TEST_CASE("Tank::format_info contains low/high level values", "[tank]")
{
    Tank tank{make_valid_config()};
    auto info = tank.format_info();
    TEST_ASSERT_NOT_NULL(std::strstr(info.c_str(), "100"));
    TEST_ASSERT_NOT_NULL(std::strstr(info.c_str(), "900"));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 8  format_info_into()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Tank::format_info_into succeeds with adequate buffer", "[tank]")
{
    Tank tank{make_valid_config()};
    tank.init();

    std::array<uint8_t, 256> buf{};
    auto r = tank.format_info_into(MutableByteView{buf});
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_GREATER_THAN(0u,
        std::strlen(reinterpret_cast<const char*>(buf.data())));
}

TEST_CASE("Tank::format_info_into returns BufferOverflow for tiny buffer", "[tank]")
{
    Tank tank{make_valid_config()};
    tank.init();

    std::array<uint8_t, 8> tiny{};
    auto r = tank.format_info_into(MutableByteView{tiny});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::BufferOverflow),
                          static_cast<int>(r.error()));
}

TEST_CASE("Tank::format_info_into returns InvalidParameter for empty span", "[tank]")
{
    Tank tank{make_valid_config()};
    auto r = tank.format_info_into(MutableByteView{});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}
