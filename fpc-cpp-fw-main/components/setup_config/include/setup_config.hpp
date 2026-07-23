/**
 * @file setup_config.hpp
 * @brief Pure data structs that mirror the JSON config schema exactly.
 *
 * These structs are filled by ConfigManager (JSON → structs) and consumed by
 * Factory (structs → wired component objects).  No hardware types, no callbacks,
 * no component headers — just plain value types.
 *
 * JSON schema (top level):
 * {
 *   "site_id": "...",
 *   "device_id": "...",
 *   "pump_control_units": [ { ... } ]
 * }
 *
 * Each pump_control_unit entry contains:
 *   pumps, tanks, relays, current_sensors, level_sensors,
 *   pump_monitors, tank_monitors, subscriptions
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fpc {

// ─── Enums ────────────────────────────────────────────────────────────────────

enum class CurrentSensorInterfaceType : uint8_t {
    ADS1115_One = 0,  ///< JSON: "ADS1115_one"
    InternalADC = 1,  ///< JSON: "internal_adc"
};

enum class CurrentSensorMakeType : uint8_t {
    ACS712 = 0,       ///< JSON: "ACS712"
};

enum class CurrentSensorReadModeType : uint8_t {
    Basic       = 0,  ///< JSON: "basic"
    Continuous  = 1,  ///< JSON: "continuous"
    Overcurrent = 2,  ///< JSON: "overcurrent"
};

enum class LevelSensorProtocolType : uint8_t {
    GA1 = 0,          ///< JSON: "GA1"
};

enum class MonitorType : uint8_t {
    TankMonitor = 0,  ///< JSON: "TANK_MONITOR"
    PumpMonitor = 1,  ///< JSON: "PUMP_MONITOR"
};

enum class SubscriberType : uint8_t {
    Relay = 0,        ///< JSON: "RELAY"
};

enum class RelayResponseType : uint8_t {
    RelayResponseOne = 0, ///< JSON: "RELAY_RESPONSE_ONE"
};

// ─── Structs — ordered innermost → outermost ──────────────────────────────────

/// JSON: current_sensors[].interface  { "type": "...", "channel": N }
struct CurrentSensorInterfaceSetupConfig {
    CurrentSensorInterfaceType type{CurrentSensorInterfaceType::InternalADC};
    int32_t                    channel{0};
};

/// JSON: current_sensors[]
struct CurrentSensorSetupConfig {
    int32_t                          id{-1};
    CurrentSensorInterfaceSetupConfig interface{};
    CurrentSensorMakeType            make{CurrentSensorMakeType::ACS712};
    float                            max_current{0.0f};
    CurrentSensorReadModeType        read_mode{CurrentSensorReadModeType::Basic};
};

/// JSON: pumps[]
struct PumpSetupConfig {
    int32_t     id{-1};
    std::string make{};
    float       power_hp{0.0f};         ///< JSON key: "power_in_hp"
    float       current_rating{0.0f};
    float       min_working_current{0.0f};
};

/// JSON: pump_monitors[]
struct PumpMonitorSetupConfig {
    int32_t id{-1};
    int32_t pump_id{-1};
    int32_t current_sensor_id{-1};
};

/// JSON: relays[]
struct RelaySetupConfig {
    int32_t id{-1};
    int32_t pin_number{-1};  ///< JSON key: "pin_number"
};

/// JSON: subscriptions[].subscribers[]
struct SubscriberSetupConfig {
    SubscriberType    type{SubscriberType::Relay};
    int32_t           id{-1};
    RelayResponseType response_type{RelayResponseType::RelayResponseOne};
};

/// JSON: subscriptions[]
struct SubscriptionSetupConfig {
    MonitorType                        monitor_type{MonitorType::PumpMonitor};
    int32_t                            monitor_id{-1};
    std::vector<SubscriberSetupConfig> subscribers;
};

/// JSON: level_sensors[]
struct LevelSensorSetupConfig {
    int32_t                 id{-1};
    int32_t                 address{0};    ///< Modbus slave address
    LevelSensorProtocolType protocol{LevelSensorProtocolType::GA1};
};

/// JSON: tanks[]
struct TankSetupConfig {
    int32_t     id{-1};
    float       capacity_litres{0.0f};
    std::string shape{};          ///< "RECTANGULAR" or "CYLINDRICAL"
    float       height_cm{0.0f};
    int32_t     full_level_mm{0};
    int32_t     low_level_mm{0};
};

/// JSON: tank_monitors[]
struct TankMonitorSetupConfig {
    int32_t id{-1};
    int32_t tank_id{-1};
    int32_t level_sensor_id{-1};
};

/// JSON: pump_control_units[]
struct PumpControlUnitSetupConfig {
    int32_t                               id{-1};
    std::vector<PumpSetupConfig>          pumps;
    std::vector<TankSetupConfig>          tanks;
    std::vector<RelaySetupConfig>         relays;
    std::vector<CurrentSensorSetupConfig> current_sensors;
    std::vector<LevelSensorSetupConfig>   level_sensors;
    std::vector<PumpMonitorSetupConfig>   pump_monitors;
    std::vector<TankMonitorSetupConfig>   tank_monitors;
    std::vector<SubscriptionSetupConfig>  subscriptions;
};

/// JSON root
struct AppSetupConfig {
    std::string                              site_id;
    std::string                              device_id;
    std::vector<PumpControlUnitSetupConfig>  pump_control_units;
};

} // namespace fpc
