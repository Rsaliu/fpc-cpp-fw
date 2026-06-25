#include "device_mode.hpp"
#include <unity.h>

using namespace fpc;

// ─── handle_event logic is testable without real GPIO ─────────────────────────
// We stub handle_event by creating a config with known callbacks and verifying
// the correct one is invoked based on the callback presence / null checks.

TEST_CASE("DeviceMode: handle_event with null main_task_cb returns NullParameter", "[device_mode]")
{
    // When button is released (level=1) but main_task_cb is null → NullParameter
    DeviceModeConfig cfg{
        .button_pin    = GPIO_NUM_16,
        .main_task_cb  = nullptr,  // intentionally null
        .webserver_cb  = []() {},
    };
    DeviceMode dm{cfg};
    // Note: handle_event calls gpio_get_level — on test hardware (no button
    // actually wired) this is unspecified, so we only test the null guard path.
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::NullParameter),
                          static_cast<int>(dm.handle_event().error()));
}

TEST_CASE("DeviceMode: handle_event with null webserver_cb does not crash", "[device_mode]")
{
    DeviceModeConfig cfg{
        .button_pin    = GPIO_NUM_16,
        .main_task_cb  = []() {},
        .webserver_cb  = nullptr,  // intentionally null
    };
    DeviceMode dm{cfg};
    // gpio_get_level on uninitialised pin on ESP32-S3 may return 1 (pull-up).
    // If it returns 1, main_task_cb is valid and we get Ok; if 0, we get NullParameter.
    auto r = dm.handle_event();
    TEST_ASSERT(r.is_ok() || r.error() == SystemError::NullParameter);
}

TEST_CASE("DeviceMode: init with GPIO_NUM_NC returns InvalidParameter", "[device_mode]")
{
    DeviceModeConfig cfg{
        .button_pin   = GPIO_NUM_NC,
        .main_task_cb = []() {},
        .webserver_cb = []() {},
    };
    DeviceMode dm{cfg};
    auto r = dm.init();
    TEST_ASSERT(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("DeviceMode: is_initialized is false before init", "[device_mode]")
{
    DeviceModeConfig cfg{
        .button_pin   = GPIO_NUM_16,
        .main_task_cb = []() {},
        .webserver_cb = []() {},
    };
    DeviceMode dm{cfg};
    TEST_ASSERT_FALSE(dm.is_initialized());
}

TEST_CASE("DeviceMode: move-constructed config does not crash on handle_event", "[device_mode]")
{
    bool called = false;
    DeviceModeConfig cfg{
        .button_pin   = GPIO_NUM_16,
        .main_task_cb = [&called]() { called = true; },
        .webserver_cb = []() {},
    };
    DeviceMode dm{std::move(cfg)};
    TEST_ASSERT_FALSE(dm.is_initialized());
    (void)dm.handle_event(); // must not crash
}
