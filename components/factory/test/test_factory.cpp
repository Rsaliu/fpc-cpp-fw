#include "factory.hpp"
#include <unity.h>

using namespace fpc;

// ─── Helpers ──────────────────────────────────────────────────────────────────

static PumpControlUnitSetupConfig make_empty_pcu() {
    return PumpControlUnitSetupConfig{.id = 1};
}

static PumpSetupConfig make_pump(int32_t id = 1) {
    return PumpSetupConfig{
        .id = id, .make = "TestPump",
        .power_hp = 2.5f, .current_rating = 10.0f, .min_working_current = 0.5f,
    };
}

static TankSetupConfig make_tank(int32_t id = 1) {
    return TankSetupConfig{
        .id = id, .capacity_litres = 1000.0f, .shape = "RECTANGULAR",
        .height_mm = 200.0f * 10, .full_level_mm = 1800, .low_level_mm = 200,
    };
}

static CurrentSensorSetupConfig make_cs(int32_t id) {
    return CurrentSensorSetupConfig{
        .id = id,
        .interface = {.type = CurrentSensorInterfaceType::InternalADC, .channel = 0},
        .make = CurrentSensorMakeType::ACS712,
        .max_current = 20.0f,
        .read_mode = CurrentSensorReadModeType::Basic,
    };
}

static LevelSensorSetupConfig make_ls(int32_t id) {
    return LevelSensorSetupConfig{.id = id, .address = 1,
                                  .protocol = LevelSensorProtocolType::GA1};
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("Factory: empty PCU creates ok with initialized control_unit", "[factory]")
{
    auto result = Factory::create_from_config(make_empty_pcu());
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_NOT_NULL(result.value().control_unit.get());
    TEST_ASSERT_EQUAL_INT(0, result.value().pumps.size());
    TEST_ASSERT_EQUAL_INT(0, result.value().tanks.size());
}

TEST_CASE("Factory: pump is created and initialized", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.pumps.push_back(make_pump(1));

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().pumps.size());
    TEST_ASSERT_EQUAL_INT(1, result.value().pumps[0]->get_config().id);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpState::Initialized),
                          static_cast<int>(result.value().pumps[0]->get_state()));
}

TEST_CASE("Factory: tank is created and initialized", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.tanks.push_back(make_tank(1));

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().tanks.size());
    TEST_ASSERT_EQUAL_INT(1, result.value().tanks[0]->get_config().id);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankState::Initialized),
                          static_cast<int>(result.value().tanks[0]->get_state()));
}

TEST_CASE("Factory: current sensor is created with correct id", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.current_sensors.push_back(make_cs(2));

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().current_sensors.size());
    TEST_ASSERT_EQUAL_INT(2, result.value().current_sensors[0]->id());
}

TEST_CASE("Factory: level sensor is created with correct id", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.level_sensors.push_back(make_ls(1));

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().level_sensors.size());
    TEST_ASSERT_EQUAL_INT(1, result.value().level_sensors[0]->id());
}

TEST_CASE("Factory: pump monitor wired to pump and sensor is initialized", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.pumps.push_back(make_pump(1));
    cfg.current_sensors.push_back(make_cs(2));
    cfg.pump_monitors.push_back({.id=1, .pump_id=1, .current_sensor_id=2});

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().pump_monitors.size());
    TEST_ASSERT_EQUAL_INT(1, result.value().pump_monitors[0]->id());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PumpMonitorState::Initialized),
                          static_cast<int>(result.value().pump_monitors[0]->state()));
}

TEST_CASE("Factory: tank monitor wired to tank and sensor is initialized", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.tanks.push_back(make_tank(1));
    cfg.level_sensors.push_back(make_ls(1));
    cfg.tank_monitors.push_back({.id=1, .tank_id=1, .level_sensor_id=1});

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().tank_monitors.size());
    TEST_ASSERT_EQUAL_INT(1, result.value().tank_monitors[0]->id());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankMonitorState::Initialized),
                          static_cast<int>(result.value().tank_monitors[0]->state()));
}

TEST_CASE("Factory: monitors are registered in control unit and loop succeeds", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.pumps.push_back(make_pump(1));
    cfg.tanks.push_back(make_tank(1));
    cfg.current_sensors.push_back(make_cs(2));
    cfg.level_sensors.push_back(make_ls(1));
    cfg.pump_monitors.push_back({.id=1, .pump_id=1, .current_sensor_id=2});
    cfg.tank_monitors.push_back({.id=1, .tank_id=1, .level_sensor_id=1});

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT(result.value().control_unit->is_initialized());
    TEST_ASSERT(result.value().control_unit->loop_pump_monitors().is_ok());
}

TEST_CASE("Factory: pump monitor with invalid pump_id returns InvalidParameter", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.current_sensors.push_back(make_cs(2));
    cfg.pump_monitors.push_back({.id=1, .pump_id=99, .current_sensor_id=2}); // 99 missing

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(result.error()));
}

TEST_CASE("Factory: pump monitor with invalid sensor_id returns error", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.pumps.push_back(make_pump(1));
    cfg.pump_monitors.push_back({.id=1, .pump_id=1, .current_sensor_id=99}); // 99 missing

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_err());
}

TEST_CASE("Factory: tank monitor with invalid tank_id returns error", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.level_sensors.push_back(make_ls(1));
    cfg.tank_monitors.push_back({.id=1, .tank_id=99, .level_sensor_id=1}); // 99 missing

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_err());
}

TEST_CASE("Factory: multiple pumps and monitors all created correctly", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.pumps.push_back(make_pump(1));
    cfg.pumps.push_back(make_pump(2));
    cfg.current_sensors.push_back(make_cs(1));
    cfg.current_sensors.push_back(make_cs(2));
    cfg.pump_monitors.push_back({.id=1, .pump_id=1, .current_sensor_id=1});
    cfg.pump_monitors.push_back({.id=2, .pump_id=2, .current_sensor_id=2});

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(2, result.value().pumps.size());
    TEST_ASSERT_EQUAL_INT(2, result.value().pump_monitors.size());
}

TEST_CASE("Factory: RECTANGULAR shape maps to TankShape::Rectangle", "[factory]")
{
    auto cfg = make_empty_pcu();
    cfg.tanks.push_back(make_tank(1)); // shape = "RECTANGULAR"

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankShape::Rectangle),
                          static_cast<int>(result.value().tanks[0]->get_config().shape));
}

TEST_CASE("Factory: CYLINDRICAL shape maps to TankShape::Cylinder", "[factory]")
{
    auto cfg = make_empty_pcu();
    TankSetupConfig t = make_tank(1);
    t.shape = "CYLINDRICAL";
    cfg.tanks.push_back(t);

    auto result = Factory::create_from_config(cfg);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(TankShape::Cylinder),
                          static_cast<int>(result.value().tanks[0]->get_config().shape));
}

