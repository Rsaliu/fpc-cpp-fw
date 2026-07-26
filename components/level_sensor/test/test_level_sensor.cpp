/**
 * @file test_level_sensor.cpp
 * @brief Unity tests for fpc::LevelSensor.
 *
 * All hardware interactions (frame builder, transport, interpreter) are
 * replaced by lightweight lambdas — no real RS-485 bus or sensor needed.
 *
 * Test coverage:
 *   §1  Helpers: stub callbacks (valid response, no-response, CRC error)
 *   §2  Construction / id()
 *   §3  init() — success, null-callback guard, double-init guard
 *   §4  deinit() — success, not-initialised guard
 *   §5  read() — success (level=754), not-initialised guard,
 *                transport failure, zero bytes_read (NoResponse),
 *                interpreter CRC failure
 *   §6  ILevelSensor polymorphic usage
 */

#include "unity.h"
#include "level_sensor.hpp"
#include "protocol.hpp"
#include <array>
#include <cstring>
#include <memory>

using namespace fpc;

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Stub callbacks
// ═══════════════════════════════════════════════════════════════════════════

// Valid GL-A01 response for level = 754 (0x02F2).
static constexpr std::array<uint8_t, 7> kValidResponse = {
    0x01u, 0x03u, 0x02u, 0x02u, 0xF2u, 0x38u, 0xA1u};

/// Transport that returns kValidResponse.
static Transport make_valid_transport()
{
    return [](ByteView, MutableByteView resp, uint32_t, int32_t& n) -> Result<void> {
        const auto copy_n = std::min(resp.size(), kValidResponse.size());
        std::memcpy(resp.data(), kValidResponse.data(), copy_n);
        n = static_cast<int32_t>(copy_n);
        return Result<void>::ok();
    };
}

/// Transport that succeeds but sets bytes_read = 0 (simulates no response).
static Transport make_no_response_transport()
{
    return [](ByteView, MutableByteView, uint32_t, int32_t& n) -> Result<void> {
        n = 0;
        return Result<void>::ok();
    };
}

/// Transport that returns a hard error.
static Transport make_failing_transport()
{
    return [](ByteView, MutableByteView, uint32_t, int32_t& n) -> Result<void> {
        n = 0;
        return Result<void>::err(SystemError::Failed);
    };
}

/// Interpreter that always returns ChecksumValidationFailed.
static ResponseInterpreter make_bad_crc_interpreter()
{
    return [](ByteView) -> Result<uint16_t> {
        return Result<uint16_t>::err(SystemError::ChecksumValidationFailed);
    };
}

/// Build a default valid LevelSensorConfig with real GL-A01 callbacks.
static LevelSensorConfig make_valid_config(Transport transport = make_valid_transport())
{
    return LevelSensorConfig{
        .id           = 1,
        .sensor_addr  = 0x01u,
        .frame_builder = [](uint8_t addr) {
            return protocol::gl_a01::build_read_level(addr);
        },
        .transport    = std::move(transport),
        .interpreter  = [](ByteView resp) {
            return protocol::gl_a01::interpret_response(resp);
        },
        .timeout_ms   = 100u,
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Construction / id()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("LevelSensor::id returns config id", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    TEST_ASSERT_EQUAL_INT(1, sensor.id());
}

TEST_CASE("LevelSensor is not active before init", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    // read() before init must fail
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  init()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("LevelSensor::init succeeds with valid config", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    TEST_ASSERT_TRUE(sensor.init().is_ok());
}

TEST_CASE("LevelSensor::init returns InvalidParameter when frame_builder is null", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.frame_builder = nullptr;
    LevelSensor sensor{std::move(cfg)};
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::init returns InvalidParameter when transport is null", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.transport = nullptr;
    LevelSensor sensor{std::move(cfg)};
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::init returns InvalidParameter when interpreter is null", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.interpreter = nullptr;
    LevelSensor sensor{std::move(cfg)};
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::init returns InvalidState when called twice", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    sensor.init();
    auto r = sensor.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  deinit()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("LevelSensor::deinit succeeds after init", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    sensor.init();
    TEST_ASSERT_TRUE(sensor.deinit().is_ok());
}

TEST_CASE("LevelSensor::deinit returns InvalidState when not initialised", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    auto r = sensor.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::read fails after deinit", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    sensor.init();
    sensor.deinit();
    TEST_ASSERT_TRUE(sensor.read().is_err());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  read()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("LevelSensor::read returns level=754 for valid response", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    sensor.init();

    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_UINT16(754u, r.value());
}

TEST_CASE("LevelSensor::read returns InvalidState when not initialised", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config()};
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::read returns Failed when transport returns error", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config(make_failing_transport())};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::Failed),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::read returns NoResponse when transport gives 0 bytes", "[level_sensor]")
{
    LevelSensor sensor{make_valid_config(make_no_response_transport())};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::NoResponse),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::read returns ChecksumValidationFailed on bad CRC", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.interpreter = make_bad_crc_interpreter();
    LevelSensor sensor{std::move(cfg)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::ChecksumValidationFailed),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor::read uses sensor_addr in frame builder", "[level_sensor]")
{
    uint8_t captured_addr = 0u;
    auto cfg = make_valid_config();
    cfg.sensor_addr   = 0x05u;
    cfg.frame_builder = [&captured_addr](uint8_t addr) {
        captured_addr = addr;
        return protocol::gl_a01::build_read_level(addr);
    };

    LevelSensor sensor{std::move(cfg)};
    sensor.init();
    sensor.read();

    TEST_ASSERT_EQUAL_HEX8(0x05u, captured_addr);
}

// ═══════════════════════════════════════════════════════════════════════════
// § 6  ILevelSensor polymorphic usage
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("ILevelSensor pointer works with LevelSensor read()", "[level_sensor]")
{
    auto concrete = std::make_unique<LevelSensor>(make_valid_config());
    ILevelSensor* sensor = concrete.get();
    sensor->init();
    auto r = sensor->read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_UINT16(754u, r.value());
}

TEST_CASE("LevelSensor: read return if it is below the blindspot", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.blindspot_mm = 800u;
    LevelSensor sensor{std::move(cfg)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidLevelReading),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor: read return if it is equal to the blindspot", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.blindspot_mm = 754u;
    LevelSensor sensor{std::move(cfg)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidLevelReading),
                          static_cast<int>(r.error()));
}

TEST_CASE("LevelSensor: read return if it is greater than the blindspot", "[level_sensor]")
{
    auto cfg = make_valid_config();
    cfg.blindspot_mm = 500u;
    LevelSensor sensor{std::move(cfg)};
    sensor.init();
    auto r = sensor.read();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_UINT16(754u, r.value());
} 