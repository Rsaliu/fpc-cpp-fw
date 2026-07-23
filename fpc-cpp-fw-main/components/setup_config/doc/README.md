# setup_config

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`setup_config` defines the **plain data structs** that mirror the device's JSON configuration schema exactly. These structs act as the bridge between the two major pipeline stages:

- **Filled by** `config_manager` (JSON → structs)
- **Consumed by** `factory` (structs → live hardware objects)

There are no hardware types, no callbacks, no FreeRTOS handles, and no component headers in this file — only value types (`int`, `float`, `std::string`, `std::vector`).

---

## Struct hierarchy

```
AppSetupConfig
└── pump_control_units: vector<PumpControlUnitSetupConfig>
        ├── id, site_id, device_id
        ├── pumps:           vector<PumpSetupConfig>
        ├── tanks:           vector<TankSetupConfig>
        ├── relays:          vector<RelaySetupConfig>
        ├── current_sensors: vector<CurrentSensorSetupConfig>
        ├── level_sensors:   vector<LevelSensorSetupConfig>
        ├── pump_monitors:   vector<PumpMonitorSetupConfig>
        ├── tank_monitors:   vector<TankMonitorSetupConfig>
        └── subscriptions:   vector<SubscriptionSetupConfig>
                └── subscribers: vector<SubscriberSetupConfig>
```

---

## Key enums

```cpp
enum class CurrentSensorInterfaceType : uint8_t {
    ADS1115_One = 0,   // JSON: "ADS1115_one"
    InternalADC = 1,   // JSON: "internal_adc"
};

enum class LevelSensorProtocolType : uint8_t {
    GA1 = 0,           // JSON: "GA1"
};

enum class MonitorType : uint8_t {
    TankMonitor = 0,   // JSON: "TANK_MONITOR"
    PumpMonitor = 1,   // JSON: "PUMP_MONITOR"
};

enum class SubscriberType : uint8_t {
    Relay = 0,         // JSON: "RELAY"
};

enum class RelayResponseType : uint8_t {
    RelayResponseOne = 0,  // JSON: "RELAY_RESPONSE_ONE"
};
```

---

## Selected struct definitions

```cpp
struct PumpSetupConfig {
    int32_t     id{-1};
    std::string make{};
    float       power_in_hp{0.0f};
    float       current_rating{0.0f};
    float       min_working_current{0.0f};
};

struct RelaySetupConfig {
    int32_t id{-1};
    int32_t pin_number{-1};
};

struct TankSetupConfig {
    int32_t     id{-1};
    float       capacity_litres{0.0f};
    std::string shape{};           // "RECTANGULAR" or "CYLINDRICAL"
    float       height_cm{0.0f};
    int32_t     full_level_mm{0};
    int32_t     low_level_mm{0};
};

struct SubscriptionSetupConfig {
    MonitorType                   monitor_type{};
    int32_t                       monitor_id{-1};
    std::vector<SubscriberSetupConfig> subscribers;
};
```

---

## Design rationale

Keeping the config structs in a separate component from both `config_manager` and `factory` means:
- `config_manager` and `factory` can evolve independently without circular dependencies.
- Unit tests for either component only need to `#include "setup_config.hpp"` — no hardware headers.
- The JSON schema is documented in one place (this component) rather than scattered across parser and builder code.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `config_t` flat struct with raw `char*` fields | Nested structs with `std::string` and `std::vector` |
| `int` for enum fields (e.g. interface type) | Typed `enum class` values |
| No clear ownership — pointers everywhere | All value types — copy-safe, no ownership issues |
