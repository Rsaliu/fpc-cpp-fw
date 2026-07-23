#include "setup_config.hpp"
#include <unity.h>

using namespace fpc;

TEST_CASE("PumpSetupConfig: fields round-trip correctly", "[setup_config]")
{
    PumpSetupConfig cfg{
        .id = 1,
        .make = "TestPump",
        .power_hp = 2.5f,
        .current_rating = 10.0f,
        .min_working_current = 0.5f,
    };
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_STRING("TestPump", cfg.make.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, cfg.power_hp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, cfg.current_rating);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, cfg.min_working_current);
}

TEST_CASE("TankSetupConfig: fields round-trip correctly", "[setup_config]")
{
    TankSetupConfig cfg{
        .id = 1,
        .capacity_litres = 1000.0f,
        .shape = "RECTANGULAR",
        .height_cm = 200.0f,
        .full_level_mm = 1800,
        .low_level_mm = 200,
    };
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000.0f, cfg.capacity_litres);
    TEST_ASSERT_EQUAL_STRING("RECTANGULAR", cfg.shape.c_str());
    TEST_ASSERT_EQUAL_INT(1800, cfg.full_level_mm);
    TEST_ASSERT_EQUAL_INT(200, cfg.low_level_mm);
}

TEST_CASE("RelaySetupConfig: fields round-trip correctly", "[setup_config]")
{
    RelaySetupConfig cfg{.id = 1, .pin_number = 23};
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_INT(23, cfg.pin_number);
}

TEST_CASE("CurrentSensorSetupConfig: ADS1115_One interface", "[setup_config]")
{
    CurrentSensorSetupConfig cfg{
        .id = 1,
        .interface = {.type = CurrentSensorInterfaceType::ADS1115_One, .channel = 1},
        .make = CurrentSensorMakeType::ACS712,
        .max_current = 20.0f,
        .read_mode = CurrentSensorReadModeType::Basic,
    };
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(cfg.interface.type)); // ADS1115_One = 0
    TEST_ASSERT_EQUAL_INT(1, cfg.interface.channel);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, cfg.max_current);
}

TEST_CASE("CurrentSensorSetupConfig: InternalADC interface", "[setup_config]")
{
    CurrentSensorSetupConfig cfg{
        .id = 2,
        .interface = {.type = CurrentSensorInterfaceType::InternalADC, .channel = 0},
        .make = CurrentSensorMakeType::ACS712,
        .max_current = 20.0f,
        .read_mode = CurrentSensorReadModeType::Basic,
    };
    TEST_ASSERT_EQUAL_INT(2, cfg.id);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(cfg.interface.type)); // InternalADC = 1
    TEST_ASSERT_EQUAL_INT(0, cfg.interface.channel);
}

TEST_CASE("LevelSensorSetupConfig: fields round-trip correctly", "[setup_config]")
{
    LevelSensorSetupConfig cfg{
        .id = 1,
        .address = 1,
        .protocol = LevelSensorProtocolType::GA1,
    };
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_INT(1, cfg.address);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(cfg.protocol)); // GA1 = 0
}

TEST_CASE("PumpMonitorSetupConfig: fields round-trip correctly", "[setup_config]")
{
    PumpMonitorSetupConfig cfg{.id = 1, .pump_id = 1, .current_sensor_id = 2};
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_INT(1, cfg.pump_id);
    TEST_ASSERT_EQUAL_INT(2, cfg.current_sensor_id);
}

TEST_CASE("TankMonitorSetupConfig: fields round-trip correctly", "[setup_config]")
{
    TankMonitorSetupConfig cfg{.id = 1, .tank_id = 1, .level_sensor_id = 1};
    TEST_ASSERT_EQUAL_INT(1, cfg.id);
    TEST_ASSERT_EQUAL_INT(1, cfg.tank_id);
    TEST_ASSERT_EQUAL_INT(1, cfg.level_sensor_id);
}

TEST_CASE("SubscriptionSetupConfig: fields round-trip correctly", "[setup_config]")
{
    SubscriptionSetupConfig cfg{
        .monitor_type = MonitorType::PumpMonitor,
        .monitor_id = 1,
        .subscribers = {
            {.type = SubscriberType::Relay,
             .id = 1,
             .response_type = RelayResponseType::RelayResponseOne},
        },
    };
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(cfg.monitor_type)); // PumpMonitor = 1
    TEST_ASSERT_EQUAL_INT(1, cfg.monitor_id);
    TEST_ASSERT_EQUAL_INT(1, cfg.subscribers.size());
    TEST_ASSERT_EQUAL_INT(1, cfg.subscribers[0].id);
}

TEST_CASE("PumpControlUnitSetupConfig: aggregates all sub-configs", "[setup_config]")
{
    PumpControlUnitSetupConfig pcu{
        .id = 1,
        .pumps           = {{.id=1, .make="P", .power_hp=1.0f, .current_rating=5.0f, .min_working_current=0.5f}},
        .tanks           = {{.id=1, .capacity_litres=1000.0f, .shape="RECTANGULAR", .height_cm=200.0f, .full_level_mm=1800, .low_level_mm=200}},
        .relays          = {{.id=1, .pin_number=23}},
        .current_sensors = {{.id=2, .interface={.type=CurrentSensorInterfaceType::InternalADC, .channel=0}, .make=CurrentSensorMakeType::ACS712, .max_current=20.0f, .read_mode=CurrentSensorReadModeType::Basic}},
        .level_sensors   = {{.id=1, .address=1, .protocol=LevelSensorProtocolType::GA1}},
        .pump_monitors   = {{.id=1, .pump_id=1, .current_sensor_id=2}},
        .tank_monitors   = {{.id=1, .tank_id=1, .level_sensor_id=1}},
        .subscriptions   = {{.monitor_type=MonitorType::PumpMonitor, .monitor_id=1, .subscribers={{.type=SubscriberType::Relay, .id=1, .response_type=RelayResponseType::RelayResponseOne}}}},
    };
    TEST_ASSERT_EQUAL_INT(1, pcu.id);
    TEST_ASSERT_EQUAL_INT(1, pcu.pumps.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.tanks.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.relays.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.current_sensors.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.level_sensors.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.pump_monitors.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.tank_monitors.size());
    TEST_ASSERT_EQUAL_INT(1, pcu.subscriptions.size());
}

TEST_CASE("AppSetupConfig: top-level fields round-trip correctly", "[setup_config]")
{
    AppSetupConfig app{
        .site_id = "Site123",
        .device_id = "Device456",
        .pump_control_units = {{.id = 1}},
    };
    TEST_ASSERT_EQUAL_STRING("Site123", app.site_id.c_str());
    TEST_ASSERT_EQUAL_STRING("Device456", app.device_id.c_str());
    TEST_ASSERT_EQUAL_INT(1, app.pump_control_units.size());
}

