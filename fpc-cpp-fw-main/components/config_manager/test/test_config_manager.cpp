#include "config_manager.hpp"
#include <unity.h>

using namespace fpc;

// Full sample JSON matching the exact schema
static const char* kSampleJson =
"{"
"  \"site_id\": \"Site123\","
"  \"device_id\": \"Device456\","
"  \"pump_control_units\": ["
"    {"
"      \"id\": 1,"
"      \"tank_monitors\": ["
"        { \"id\": 1, \"tank_id\": 1, \"level_sensor_id\": 1 }"
"      ],"
"      \"pump_monitors\": ["
"        { \"id\": 1, \"pump_id\": 1, \"current_sensor_id\": 2 }"
"      ],"
"      \"tanks\": ["
"        {"
"          \"id\": 1,"
"          \"capacity_litres\": 1000.0,"
"          \"shape\": \"RECTANGULAR\","
"          \"height_cm\": 200.0,"
"          \"full_level_mm\": 1800,"
"          \"low_level_mm\": 200"
"        }"
"      ],"
"      \"pumps\": ["
"        {"
"          \"id\": 1,"
"          \"make\": \"TestPump\","
"          \"power_in_hp\": 2.5,"
"          \"current_rating\": 10.0,"
"          \"min_working_current\": 0.5"
"        }"
"      ],"
"      \"relays\": ["
"        { \"id\": 1, \"pin_number\": 23 }"
"      ],"
"      \"current_sensors\": ["
"        {"
"          \"id\": 1,"
"          \"interface\": { \"type\": \"ADS1115_one\", \"channel\": 1 },"
"          \"make\": \"ACS712\","
"          \"max_current\": 20,"
"          \"read_mode\": \"basic\""
"        },"
"        {"
"          \"id\": 2,"
"          \"interface\": { \"type\": \"internal_adc\", \"channel\": 0 },"
"          \"make\": \"ACS712\","
"          \"max_current\": 20,"
"          \"read_mode\": \"basic\""
"        }"
"      ],"
"      \"level_sensors\": ["
"        {"
"          \"id\": 1,"
"          \"interface\": \"RS485\","
"          \"address\": 1,"
"          \"protocol\": \"GA1\""
"        }"
"      ],"
"      \"subscriptions\": ["
"        {"
"          \"monitor_type\": \"PUMP_MONITOR\","
"          \"monitor_id\": 1,"
"          \"subscribers\": ["
"            {"
"              \"type\": \"RELAY\","
"              \"id\": 1,"
"              \"response_type\": \"RELAY_RESPONSE_ONE\""
"            }"
"          ]"
"        }"
"      ]"
"    }"
"  ]"
"}";

TEST_CASE("parse: top-level site_id, device_id, pcu count", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_STRING("Site123",   result.value().site_id.c_str());
    TEST_ASSERT_EQUAL_STRING("Device456", result.value().device_id.c_str());
    TEST_ASSERT_EQUAL_INT(1, result.value().pump_control_units.size());
}

TEST_CASE("parse: pcu id", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    TEST_ASSERT_EQUAL_INT(1, result.value().pump_control_units[0].id);
}

TEST_CASE("parse: pumps fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& pumps = result.value().pump_control_units[0].pumps;
    TEST_ASSERT_EQUAL_INT(1, pumps.size());
    TEST_ASSERT_EQUAL_INT(1, pumps[0].id);
    TEST_ASSERT_EQUAL_STRING("TestPump", pumps[0].make.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, pumps[0].power_hp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, pumps[0].current_rating);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, pumps[0].min_working_current);
}

TEST_CASE("parse: tanks fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& tanks = result.value().pump_control_units[0].tanks;
    TEST_ASSERT_EQUAL_INT(1, tanks.size());
    TEST_ASSERT_EQUAL_INT(1, tanks[0].id);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1000.0f, tanks[0].capacity_litres);
    TEST_ASSERT_EQUAL_STRING("RECTANGULAR", tanks[0].shape.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 200.0f, tanks[0].height_cm);
    TEST_ASSERT_EQUAL_INT(1800, tanks[0].full_level_mm);
    TEST_ASSERT_EQUAL_INT(200,  tanks[0].low_level_mm);
}

TEST_CASE("parse: relays fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& relays = result.value().pump_control_units[0].relays;
    TEST_ASSERT_EQUAL_INT(1, relays.size());
    TEST_ASSERT_EQUAL_INT(1, relays[0].id);
    TEST_ASSERT_EQUAL_INT(23, relays[0].pin_number);
}

TEST_CASE("parse: current sensors fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& cs = result.value().pump_control_units[0].current_sensors;
    TEST_ASSERT_EQUAL_INT(2, cs.size());

    // First sensor: ADS1115_one, channel 1
    TEST_ASSERT_EQUAL_INT(1, cs[0].id);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(cs[0].interface.type)); // ADS1115_One
    TEST_ASSERT_EQUAL_INT(1, cs[0].interface.channel);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, cs[0].max_current);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(cs[0].read_mode)); // Basic

    // Second sensor: internal_adc, channel 0
    TEST_ASSERT_EQUAL_INT(2, cs[1].id);
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(cs[1].interface.type)); // InternalADC
    TEST_ASSERT_EQUAL_INT(0, cs[1].interface.channel);
}

TEST_CASE("parse: level sensors fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& ls = result.value().pump_control_units[0].level_sensors;
    TEST_ASSERT_EQUAL_INT(1, ls.size());
    TEST_ASSERT_EQUAL_INT(1, ls[0].id);
    TEST_ASSERT_EQUAL_INT(1, ls[0].address);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(ls[0].protocol)); // GA1
}

TEST_CASE("parse: pump monitors fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& pm = result.value().pump_control_units[0].pump_monitors;
    TEST_ASSERT_EQUAL_INT(1, pm.size());
    TEST_ASSERT_EQUAL_INT(1, pm[0].id);
    TEST_ASSERT_EQUAL_INT(1, pm[0].pump_id);
    TEST_ASSERT_EQUAL_INT(2, pm[0].current_sensor_id);
}

TEST_CASE("parse: tank monitors fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& tm = result.value().pump_control_units[0].tank_monitors;
    TEST_ASSERT_EQUAL_INT(1, tm.size());
    TEST_ASSERT_EQUAL_INT(1, tm[0].id);
    TEST_ASSERT_EQUAL_INT(1, tm[0].tank_id);
    TEST_ASSERT_EQUAL_INT(1, tm[0].level_sensor_id);
}

TEST_CASE("parse: subscriptions fields", "[config_manager]")
{
    auto result = ConfigManager::parse(kSampleJson);
    TEST_ASSERT(result.is_ok());
    const auto& subs = result.value().pump_control_units[0].subscriptions;
    TEST_ASSERT_EQUAL_INT(1, subs.size());
    TEST_ASSERT_EQUAL_INT(1, static_cast<int>(subs[0].monitor_type)); // PumpMonitor
    TEST_ASSERT_EQUAL_INT(1, subs[0].monitor_id);
    TEST_ASSERT_EQUAL_INT(1, subs[0].subscribers.size());
    TEST_ASSERT_EQUAL_INT(1, subs[0].subscribers[0].id);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(subs[0].subscribers[0].type)); // Relay
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(subs[0].subscribers[0].response_type)); // RelayResponseOne
}

TEST_CASE("parse: invalid JSON returns InvalidParameter", "[config_manager]")
{
    auto result = ConfigManager::parse("{ not valid json !!!");
    TEST_ASSERT(result.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(result.error()));
}

TEST_CASE("parse: empty arrays in PCU succeed", "[config_manager]")
{
    const char* json = "{\"site_id\":\"S\",\"device_id\":\"D\","
                       "\"pump_control_units\":[{\"id\":1}]}";
    auto result = ConfigManager::parse(json);
    TEST_ASSERT(result.is_ok());
    const auto& pcu = result.value().pump_control_units[0];
    TEST_ASSERT_EQUAL_INT(0, pcu.pumps.size());
    TEST_ASSERT_EQUAL_INT(0, pcu.tanks.size());
    TEST_ASSERT_EQUAL_INT(0, pcu.relays.size());
}