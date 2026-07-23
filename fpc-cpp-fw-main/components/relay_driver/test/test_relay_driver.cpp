/**
 * @file test_relay_driver.cpp
 * @brief Unity tests for fpc::Relay using an injected MockGpioDriver.
 *
 * Because IGpioDriver is injected, no real GPIO hardware is needed — all
 * tests run in the FreeRTOS/ESP-IDF software environment only.
 *
 * Test coverage (mirrors reference test_relay_driver.c):
 *   §1  MockGpioDriver helper class
 *   §2  Construction / config
 *   §3  init() — success, invalid id, GPIO failure
 *   §4  deinit() — success, not-initialised guard
 *   §5  on() / off() — success, state transitions, tripped guard
 *   §6  trip() — success, idempotent on already-tripped
 *   §7  reset() / reset_and_on()
 *   §8  get_state() — state tracking, not-initialised guard
 *   §9  RAII destructor drives GPIO LOW
 */

#include "unity.h"
#include "relay_driver.hpp"
#include <memory>
#include <cstdint>

using namespace fpc;

// ═══════════════════════════════════════════════════════════════════════════
// § 1  MockGpioDriver
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Software-only GPIO mock — records all calls and returns a
 *        configurable `esp_err_t` (default: ESP_OK).
 */
class MockGpioDriver final : public IGpioDriver {
public:
    int       reset_count{0};
    int       set_dir_count{0};
    int       set_level_count{0};
    int       last_level{-1};
    esp_err_t return_code{ESP_OK};  ///< Override to simulate GPIO failure.

    esp_err_t reset_pin(gpio_num_t) override
    {
        ++reset_count;
        return return_code;
    }
    esp_err_t set_direction(gpio_num_t, gpio_mode_t) override
    {
        ++set_dir_count;
        return return_code;
    }
    esp_err_t set_level(gpio_num_t, uint32_t level) override
    {
        last_level = static_cast<int>(level);
        ++set_level_count;
        return return_code;
    }
};

// Convenience: build a relay on GPIO_NUM_5 with id=1.
static constexpr RelayConfig kDefaultConfig{.id = 1, .pin = GPIO_NUM_5};

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Construction / config
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::get_config returns config passed at construction", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};

    RelayConfig cfg = relay.get_config();
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_INT(GPIO_NUM_5, static_cast<int>(cfg.pin));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  init()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::init succeeds with valid config", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};

    TEST_ASSERT_TRUE(relay.init().is_ok());
    // GPIO should have been reset, direction set, and level driven LOW.
    TEST_ASSERT_EQUAL_INT(1, gpio.reset_count);
    TEST_ASSERT_EQUAL_INT(1, gpio.set_dir_count);
    TEST_ASSERT_EQUAL_INT(0, gpio.last_level);
}

TEST_CASE("Relay::init returns InvalidParameter for id < 0", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{RelayConfig{.id = -1, .pin = GPIO_NUM_5}, gpio};

    auto r = relay.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Relay::init returns Failed when GPIO reset_pin fails", "[relay]")
{
    MockGpioDriver gpio;
    gpio.return_code = ESP_FAIL;
    Relay relay{kDefaultConfig, gpio};

    auto r = relay.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::Failed),
                          static_cast<int>(r.error()));
}

TEST_CASE("Relay::get_state after init returns Off", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();

    auto r = relay.get_state();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::Off),
                          static_cast<int>(r.value()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  deinit()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::deinit succeeds after init", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();

    TEST_ASSERT_TRUE(relay.deinit().is_ok());
    // After deinit, get_state should return InvalidState.
    TEST_ASSERT_TRUE(relay.get_state().is_err());
}

TEST_CASE("Relay::deinit returns InvalidState when not initialised", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};

    auto r = relay.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  on() / off()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::on sets state to On and drives GPIO HIGH", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();

    TEST_ASSERT_TRUE(relay.on().is_ok());
    TEST_ASSERT_EQUAL_INT(1, gpio.last_level);

    auto st = relay.get_state();
    TEST_ASSERT_TRUE(st.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::On),
                          static_cast<int>(st.value()));
}

TEST_CASE("Relay::off sets state to Off and drives GPIO LOW", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.on();

    TEST_ASSERT_TRUE(relay.off().is_ok());
    TEST_ASSERT_EQUAL_INT(0, gpio.last_level);

    auto st = relay.get_state();
    TEST_ASSERT_TRUE(st.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::Off),
                          static_cast<int>(st.value()));
}

TEST_CASE("Relay::on returns InvalidState when not initialised", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    TEST_ASSERT_TRUE(relay.on().is_err());
}

TEST_CASE("Relay::on blocked when relay is tripped", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.trip();

    auto r = relay.on();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
    // State must remain Tripped.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::Tripped),
                          static_cast<int>(relay.get_state().value()));
}

TEST_CASE("Relay::off blocked when relay is tripped", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.trip();

    auto r = relay.off();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 6  trip()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::trip drives GPIO LOW and sets state to Tripped", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.on();

    TEST_ASSERT_TRUE(relay.trip().is_ok());
    TEST_ASSERT_EQUAL_INT(0, gpio.last_level);

    auto st = relay.get_state();
    TEST_ASSERT_TRUE(st.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::Tripped),
                          static_cast<int>(st.value()));
}

TEST_CASE("Relay::trip is idempotent — second call returns Ok", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.trip();

    // Second trip must succeed (no-op) and state stays Tripped.
    TEST_ASSERT_TRUE(relay.trip().is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::Tripped),
                          static_cast<int>(relay.get_state().value()));
}

TEST_CASE("Relay::trip returns InvalidState when not initialised", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    TEST_ASSERT_TRUE(relay.trip().is_err());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 7  reset() / reset_and_on()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::reset clears Tripped state to Off", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.trip();

    TEST_ASSERT_TRUE(relay.reset().is_ok());

    auto st = relay.get_state();
    TEST_ASSERT_TRUE(st.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::Off),
                          static_cast<int>(st.value()));
}

TEST_CASE("Relay::reset_and_on clears fault and energises relay", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    relay.init();
    relay.trip();

    TEST_ASSERT_TRUE(relay.reset_and_on().is_ok());
    TEST_ASSERT_EQUAL_INT(1, gpio.last_level);

    auto st = relay.get_state();
    TEST_ASSERT_TRUE(st.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RelayState::On),
                          static_cast<int>(st.value()));
}

TEST_CASE("Relay::reset returns InvalidState when not initialised", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};
    TEST_ASSERT_TRUE(relay.reset().is_err());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 8  get_state() guards
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay::get_state returns InvalidState when not initialised", "[relay]")
{
    MockGpioDriver gpio;
    Relay relay{kDefaultConfig, gpio};

    auto r = relay.get_state();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 9  RAII — destructor drives GPIO LOW
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Relay destructor drives GPIO LOW when initialised", "[relay]")
{
    MockGpioDriver gpio;
    {
        Relay relay{kDefaultConfig, gpio};
        relay.init();
        relay.on();
        // last_level is 1 (HIGH) at this point.
        TEST_ASSERT_EQUAL_INT(1, gpio.last_level);
    }  // relay destructs here
    // Destructor must have driven the pin LOW.
    TEST_ASSERT_EQUAL_INT(0, gpio.last_level);
}

TEST_CASE("Relay destructor does nothing when not initialised", "[relay]")
{
    MockGpioDriver gpio;
    const int pre_count = gpio.set_level_count;
    {
        Relay relay{kDefaultConfig, gpio};
        // No init() call.
    }
    // set_level must not have been called by the destructor.
    TEST_ASSERT_EQUAL_INT(pre_count, gpio.set_level_count);
}
