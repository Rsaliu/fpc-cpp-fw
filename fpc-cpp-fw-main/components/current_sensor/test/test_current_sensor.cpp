/**
 * @file test_current_sensor.cpp
 * @brief Unity tests for fpc::CurrentSensor.
 *
 * All hardware is replaced by lambda stubs — no ADC, I2C, or ACS712/ADS1115
 * hardware is needed.
 *
 * Test coverage:
 *   §1  Construction / id() / make()
 *   §2  init() — success, null callback, empty make, id<0, double-init
 *   §3  deinit() — success, not-initialised guard
 *   §4  read() — success (10.0 A), not-initialised, callback error
 *   §5  Callback variation — different current values, error propagation
 *   §6  ICurrentSensor polymorphic usage
 */

#include "unity.h"
#include "current_sensor.hpp"
#include <memory>
#include <cstring>

using namespace fpc;

// ─── Helpers ──────────────────────────────────────────────────────────────

static CurrentSensorConfig make_valid_config(float amps = 10.0f)
{
    return CurrentSensorConfig{
        .id      = 1,
        .make    = "DummySensor",
        .read_cb = [amps]() -> Result<float> {
            return Result<float>::ok(amps);
        },
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Construction / id() / make()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("CurrentSensor::id returns config id", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    TEST_ASSERT_EQUAL_INT(1, sensor.id());
}

TEST_CASE("CurrentSensor::make returns config make string", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    TEST_ASSERT_EQUAL_STRING("DummySensor", std::string{sensor.make()}.c_str());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  init()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("CurrentSensor::init succeeds with valid config", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    TEST_ASSERT_TRUE(sensor.init().is_ok());
}

TEST_CASE("CurrentSensor::init returns InvalidParameter when read_cb is null", "[current_sensor]")
{
    CurrentSensorConfig cfg = make_valid_config();
    cfg.read_cb = nullptr;
    CurrentSensor sensor{std::move(cfg)};
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("CurrentSensor::init returns InvalidParameter when make is empty", "[current_sensor]")
{
    CurrentSensorConfig cfg = make_valid_config();
    cfg.make = "";
    CurrentSensor sensor{std::move(cfg)};
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("CurrentSensor::init returns InvalidParameter when id < 0", "[current_sensor]")
{
    CurrentSensorConfig cfg = make_valid_config();
    cfg.id = -1;
    CurrentSensor sensor{std::move(cfg)};
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("CurrentSensor::init returns InvalidState when called twice", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    sensor.init();
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  deinit()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("CurrentSensor::deinit succeeds after init", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    sensor.init();
    TEST_ASSERT_TRUE(sensor.deinit().is_ok());
}

TEST_CASE("CurrentSensor::deinit returns InvalidState when not initialised", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    auto r = sensor.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("CurrentSensor::read fails after deinit", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    sensor.init();
    sensor.deinit();
    TEST_ASSERT_TRUE(sensor.read().is_err());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  read()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("CurrentSensor::read returns 10.0 A from stub callback", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config(10.0f)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, r.value());
}

TEST_CASE("CurrentSensor::read returns InvalidState when not initialised", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config()};
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("CurrentSensor::read propagates error from callback", "[current_sensor]")
{
    CurrentSensorConfig cfg{
        .id      = 1,
        .make    = "FaultySensor",
        .read_cb = []() -> Result<float> {
            return Result<float>::err(SystemError::Failed);
        },
    };
    CurrentSensor sensor{std::move(cfg)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::Failed),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  Callback variation
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("CurrentSensor::read returns 3.5 A when callback provides 3.5", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config(3.5f)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.5f, r.value());
}

TEST_CASE("CurrentSensor::read returns 0.0 A (zero current)", "[current_sensor]")
{
    CurrentSensor sensor{make_valid_config(0.0f)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.value());
}

TEST_CASE("CurrentSensor callback captures mutable state (counter)", "[current_sensor]")
{
    int call_count = 0;
    CurrentSensorConfig cfg{
        .id      = 2,
        .make    = "Counter",
        .read_cb = [&call_count]() -> Result<float> {
            ++call_count;
            return Result<float>::ok(static_cast<float>(call_count));
        },
    };
    CurrentSensor sensor{std::move(cfg)};
    sensor.init();

    auto r1 = sensor.read();
    auto r2 = sensor.read();
    TEST_ASSERT_TRUE(r1.is_ok());
    TEST_ASSERT_TRUE(r2.is_ok());
    TEST_ASSERT_EQUAL_INT(2, call_count);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r1.value());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, r2.value());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 6  ICurrentSensor polymorphic usage
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("ICurrentSensor pointer reads correctly via CurrentSensor", "[current_sensor]")
{
    auto concrete = std::make_unique<CurrentSensor>(make_valid_config(5.0f));
    ICurrentSensor* sensor = concrete.get();
    sensor->init();
    auto r = sensor->read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, r.value());
}

TEST_CASE("ICurrentSensor id() and make() accessible via interface", "[current_sensor]")
{
    auto concrete = std::make_unique<CurrentSensor>(make_valid_config());
    ICurrentSensor* sensor = concrete.get();
    TEST_ASSERT_EQUAL_INT(1, sensor->id());
    TEST_ASSERT_EQUAL_STRING("DummySensor", std::string{sensor->make()}.c_str());
}
