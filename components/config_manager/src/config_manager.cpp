#include "config_manager.hpp"
#include "cJSON.h"
#include <cstdio>
#include <cstring>

namespace fpc {

// ─── cJSON RAII wrapper ───────────────────────────────────────────────────────

struct CJsonGuard {
    cJSON* ptr{nullptr};
    explicit CJsonGuard(cJSON* p) noexcept : ptr{p} {}
    ~CJsonGuard() { if (ptr) { cJSON_Delete(ptr); } }
    CJsonGuard(const CJsonGuard&) = delete;
    CJsonGuard& operator=(const CJsonGuard&) = delete;
};

// ─── Parse helpers ────────────────────────────────────────────────────────────

static std::string safe_string(const cJSON* obj, const char* key) noexcept {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        return item->valuestring;
    }
    return {};
}

static int32_t safe_int(const cJSON* obj, const char* key, int32_t fallback = -1) noexcept {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return static_cast<int32_t>(item->valuedouble);
    }
    return fallback;
}

static float safe_float(const cJSON* obj, const char* key, float fallback = 0.0f) noexcept {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return static_cast<float>(item->valuedouble);
    }
    return fallback;
}

// ─── Per-type parsers ─────────────────────────────────────────────────────────

static PumpSetupConfig parse_pump(const cJSON* obj) noexcept {
    return PumpSetupConfig{
        .id                  = safe_int(obj, "id"),
        .make                = safe_string(obj, "make"),
        .power_hp            = safe_float(obj, "power_in_hp"),
        .current_rating      = safe_float(obj, "current_rating"),
        .min_working_current = safe_float(obj, "min_working_current"),
    };
}

static TankSetupConfig parse_tank(const cJSON* obj) noexcept {
    return TankSetupConfig{
        .id               = safe_int(obj, "id"),
        .capacity_litres  = safe_float(obj, "capacity_litres"),
        .shape            = safe_string(obj, "shape"),
        .height_mm       = safe_float(obj, "height_mm"),
        .full_level_mm    = safe_int(obj, "full_level_mm", 0),
        .low_level_mm     = safe_int(obj, "low_level_mm", 0),
    };
}

static RelaySetupConfig parse_relay(const cJSON* obj) noexcept {
    return RelaySetupConfig{
        .id         = safe_int(obj, "id"),
        .pin_number = safe_int(obj, "pin_number"),
    };
}

static CurrentSensorSetupConfig parse_current_sensor(const cJSON* obj) noexcept {
    CurrentSensorSetupConfig cfg{};
    cfg.id          = safe_int(obj, "id");
    cfg.max_current = safe_float(obj, "max_current");

    // Parse nested interface object: { "type": "...", "channel": N }
    const cJSON* iface = cJSON_GetObjectItemCaseSensitive(obj, "interface");
    if (cJSON_IsObject(iface)) {
        std::string type_str = safe_string(iface, "type");
        cfg.interface.channel = safe_int(iface, "channel", 0);
        if (type_str == "ADS1115_one") {
            cfg.interface.type = CurrentSensorInterfaceType::ADS1115_One;
        } else {
            cfg.interface.type = CurrentSensorInterfaceType::InternalADC;
        }
    }

    // Parse make
    std::string make_str = safe_string(obj, "make");
    cfg.make = (make_str == "ACS712") ? CurrentSensorMakeType::ACS712
                                      : CurrentSensorMakeType::ACS712; // default

    // Parse read_mode
    std::string mode_str = safe_string(obj, "read_mode");
    if (mode_str == "continuous") {
        cfg.read_mode = CurrentSensorReadModeType::Continuous;
    } else if (mode_str == "overcurrent") {
        cfg.read_mode = CurrentSensorReadModeType::Overcurrent;
    } else {
        cfg.read_mode = CurrentSensorReadModeType::Basic;
    }

    return cfg;
}

static LevelSensorSetupConfig parse_level_sensor(const cJSON* obj) noexcept {
    LevelSensorSetupConfig cfg{};
    cfg.id      = safe_int(obj, "id");
    cfg.address = safe_int(obj, "address", 0);

    std::string proto_str = safe_string(obj, "protocol");
    cfg.protocol = (proto_str == "GA1") ? LevelSensorProtocolType::GA1
                                        : LevelSensorProtocolType::GA1; // default
    return cfg;
}

static PumpMonitorSetupConfig parse_pump_monitor(const cJSON* obj) noexcept {
    return PumpMonitorSetupConfig{
        .id               = safe_int(obj, "id"),
        .pump_id          = safe_int(obj, "pump_id"),
        .current_sensor_id = safe_int(obj, "current_sensor_id"),
    };
}

static TankMonitorSetupConfig parse_tank_monitor(const cJSON* obj) noexcept {
    return TankMonitorSetupConfig{
        .id              = safe_int(obj, "id"),
        .tank_id         = safe_int(obj, "tank_id"),
        .level_sensor_id = safe_int(obj, "level_sensor_id"),
    };
}

static SubscriberSetupConfig parse_subscriber(const cJSON* obj) noexcept {
    SubscriberSetupConfig cfg{};
    cfg.id = safe_int(obj, "id");

    std::string type_str = safe_string(obj, "type");
    cfg.type = (type_str == "RELAY") ? SubscriberType::Relay : SubscriberType::Relay;

    std::string resp_str = safe_string(obj, "response_type");
    cfg.response_type = (resp_str == "RELAY_RESPONSE_ONE")
                        ? RelayResponseType::RelayResponseOne
                        : RelayResponseType::RelayResponseOne;
    return cfg;
}

static SubscriptionSetupConfig parse_subscription(const cJSON* obj) noexcept {
    SubscriptionSetupConfig cfg{};
    cfg.monitor_id = safe_int(obj, "monitor_id");

    std::string type_str = safe_string(obj, "monitor_type");
    cfg.monitor_type = (type_str == "PUMP_MONITOR") ? MonitorType::PumpMonitor
                                                     : MonitorType::TankMonitor;

    const cJSON* subs = cJSON_GetObjectItemCaseSensitive(obj, "subscribers");
    const cJSON* sub  = nullptr;
    cJSON_ArrayForEach(sub, subs) {
        cfg.subscribers.push_back(parse_subscriber(sub));
    }
    return cfg;
}

// ─── Helper: parse one array section ─────────────────────────────────────────

template <typename T>
static std::vector<T> parse_array(const cJSON* parent,
                                  const char* key,
                                  T(*parse_fn)(const cJSON*)) noexcept {
    std::vector<T> out;
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(parent, key);
    const cJSON* item = nullptr;
    cJSON_ArrayForEach(item, arr) {
        out.push_back(parse_fn(item));
    }
    return out;
}

// ─── PCU parser ───────────────────────────────────────────────────────────────

static PumpControlUnitSetupConfig parse_pcu(const cJSON* obj) noexcept {
    PumpControlUnitSetupConfig pcu{};
    pcu.id              = safe_int(obj, "id");
    pcu.pumps           = parse_array(obj, "pumps",           parse_pump);
    pcu.tanks           = parse_array(obj, "tanks",           parse_tank);
    pcu.relays          = parse_array(obj, "relays",          parse_relay);
    pcu.current_sensors = parse_array(obj, "current_sensors", parse_current_sensor);
    pcu.level_sensors   = parse_array(obj, "level_sensors",   parse_level_sensor);
    pcu.pump_monitors   = parse_array(obj, "pump_monitors",   parse_pump_monitor);
    pcu.tank_monitors   = parse_array(obj, "tank_monitors",   parse_tank_monitor);
    pcu.subscriptions   = parse_array(obj, "subscriptions",   parse_subscription);
    return pcu;
}

// ─── Public API ───────────────────────────────────────────────────────────────

Result<AppSetupConfig> ConfigManager::parse(std::string_view json_str) noexcept {
    CJsonGuard guard{cJSON_ParseWithLength(json_str.data(),
                                          json_str.size())};
    if (!guard.ptr) {
        return Result<AppSetupConfig>::err(SystemError::InvalidParameter);
    }

    AppSetupConfig app{};
    app.site_id   = safe_string(guard.ptr, "site_id");
    app.device_id = safe_string(guard.ptr, "device_id");

    const cJSON* pcus = cJSON_GetObjectItemCaseSensitive(guard.ptr,
                                                          "pump_control_units");
    const cJSON* pcu  = nullptr;
    cJSON_ArrayForEach(pcu, pcus) {
        app.pump_control_units.push_back(parse_pcu(pcu));
    }

    return Result<AppSetupConfig>::ok(std::move(app));
}

Result<std::string> ConfigManager::read_file(const char* path) noexcept {
    if (!path) {
        return Result<std::string>::err(SystemError::NullParameter);
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        return Result<std::string>::err(SystemError::Failed);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return Result<std::string>::err(SystemError::InvalidParameter);
    }

    std::string buf(static_cast<size_t>(size), '\0');
    fread(buf.data(), 1, static_cast<size_t>(size), f);
    fclose(f);

    return Result<std::string>::ok(std::move(buf));
}

} // namespace fpc
