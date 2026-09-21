/**
 * @file main.cpp
 * @brief Application entry point — C++17 re-write of fpc/main/main.c
 *
 * Startup flow:
 *   1. DeviceMode reads the boot button GPIO.
 *   2. Button pressed  → webserver_task() starts the HTTP config server.
 *   3. Button released → main_tasks() parses config.json, wires all components
 *                        via Factory, then starts PumpMonitorTask +
 *                        TankMonitorTask polling loops.
 */

#include "config_manager.hpp"
#include "factory.hpp"
#include "device_mode.hpp"
#include "pump_monitor_task.hpp"
#include "tank_monitor_task.hpp"
#include "wifi_hotspot.hpp"
#include "webserver_task.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace fpc;

static const char* TAG = "APP_MAIN";
constexpr int pump_monitor_check_interval_ms = 5;
constexpr int tank_monitor_check_interval_ms = 1000;

// Sample json to be used now. In the final version, the json will
// read from flash.

static const char* kDefaultJson =
    "{"
    "  \"site_id\": \"Site123\","
    "  \"device_id\": \"Device456\","
    "  \"pump_control_units\": ["
    "    {"
    "      \"id\": 1,"
    "      \"tank_monitors\":   [{ \"id\": 1, \"tank_id\": 1, \"level_sensor_id\": 1 }],"
    "      \"pump_monitors\":   [{ \"id\": 1, \"pump_id\": 1, \"current_sensor_id\": 1 }],"
    "      \"tanks\": [{"
    "          \"id\": 1, \"capacity_litres\": 1000.0, \"shape\": \"RECTANGULAR\","
    "          \"height_mm\": 2000.0, \"full_level_mm\": 1800, \"low_level_mm\": 200"
    "      }],"
    "      \"pumps\": [{"
    "          \"id\": 1, \"make\": \"TestPump\","
    "          \"power_in_hp\": 2.5, \"current_rating\": 10.0, \"min_working_current\": 0.5"
    "      }],"
    "      \"relays\":          [{ \"id\": 1, \"pin_number\": 4 }],"
    "      \"current_sensors\": [{"
    "          \"id\": 1,"
    "          \"interface\": { \"type\": \"ADS1115_one\", \"channel\": 1 },"
    "          \"make\": \"ACS712\", \"max_current\": 20, \"read_mode\": \"basic\""
    "      }],"
    "      \"level_sensors\": [{"
    "          \"id\": 1, \"interface\": \"RS485\", \"address\": 1, \"protocol\": \"GA1\""
    "      }],"
    "      \"subscriptions\": [{"
    "          \"monitor_type\": \"PUMP_MONITOR\", \"monitor_id\": 1,"
    "          \"subscribers\": [{ \"type\": \"RELAY\", \"id\": 1,"
    "                              \"response_type\": \"RELAY_RESPONSE_ONE\" }]"
    "      }]"
    "    }"
    "  ]"
    "}";

struct RuntimeContext {
    std::vector<Application> apps;
    std::vector<std::unique_ptr<PumpMonitorTask>> pump_tasks;
    std::vector<std::unique_ptr<TankMonitorTask>> tank_tasks;
    bool started{false};
};

static RuntimeContext g_runtime;



static Result<void> setup_system_from_config(const AppSetupConfig& app_cfg) {
    if (app_cfg.pump_control_units.empty()) {
        ESP_LOGE(TAG, "No pump_control_units in config");
        return Result<void>::err(SystemError::InvalidParameter);
    }

    g_runtime.apps.clear();
    g_runtime.apps.reserve(app_cfg.pump_control_units.size());

    for (const auto& pcu_cfg : app_cfg.pump_control_units) {
        auto app_result = Factory::create_from_config(pcu_cfg);
        if (app_result.is_err()) {
            return Result<void>::err(app_result.error());
        }
        g_runtime.apps.push_back(std::move(app_result.value()));
    }

    return Result<void>::ok();
}

static Result<void> setup_tasks_from_config() {
    g_runtime.pump_tasks.clear();
    g_runtime.tank_tasks.clear();

    // Counters (not the PCU/monitor index) so task ids stay unique across
    // multiple pump_control_units — the ids are only used for task naming and
    // log correlation, but must not collide between PCUs.
    int32_t next_pump_task_id = 1;
    int32_t next_tank_task_id = 1;

    for (size_t i = 0; i < g_runtime.apps.size(); ++i) {
        auto& app = g_runtime.apps[i];

        for (size_t j = 0; j < app.pump_monitors.size(); ++j) {
            PumpMonitorTaskConfig pm_cfg;
            pm_cfg.id                = next_pump_task_id++;
            pm_cfg.monitor           = app.pump_monitors[j].get();
            pm_cfg.check_interval_ms = pump_monitor_check_interval_ms;

            auto task = std::make_unique<PumpMonitorTask>(pm_cfg);
            auto start_result = task->start();
            if (start_result.is_err()) {
                return Result<void>::err(start_result.error());
            }
            g_runtime.pump_tasks.push_back(std::move(task));
        }

        if (!app.tank_monitors.empty()) {
            TankMonitorTaskConfig tm_cfg;
            tm_cfg.id                = next_tank_task_id++;
            tm_cfg.check_interval_ms = tank_monitor_check_interval_ms;
            tm_cfg.monitors.reserve(app.tank_monitors.size());

            for (auto& tm : app.tank_monitors) {
                tm_cfg.monitors.push_back(tm.get());
            }

            auto task = std::make_unique<TankMonitorTask>(std::move(tm_cfg));
            auto start_result = task->start();
            if (start_result.is_err()) {
                return Result<void>::err(start_result.error());
            }
            g_runtime.tank_tasks.push_back(std::move(task));
        }
    }

    return Result<void>::ok();
}

// ─── Normal operation ─────────────────────────────────────────────────────────

// A failed setup must not leave the device idle with no pump/tank protection
// running, so restart and retry rather than returning inert from app_main.
[[noreturn]] static void restart_after_setup_failure() {
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

static void main_tasks_starter() {
    if (g_runtime.started) {
        ESP_LOGW(TAG, "Main tasks already started");
        return;
    }

    auto cfg_result = ConfigManager::parse(kDefaultJson);;
    if (cfg_result.is_err()) {
        ESP_LOGE(TAG, "Config parse failed: %s",
                 to_string(cfg_result.error()).data());
        restart_after_setup_failure();
    }

    // 2. Setup full system object graph from config
    if (auto r = setup_system_from_config(cfg_result.value()); r.is_err()) {
        ESP_LOGE(TAG, "System setup failed: %s", to_string(r.error()).data());
        restart_after_setup_failure();
    }

    // 3. Setup and start tasks from config
    if (auto r = setup_tasks_from_config(); r.is_err()) {
        ESP_LOGE(TAG, "Task setup failed: %s", to_string(r.error()).data());
        restart_after_setup_failure();
    }

    g_runtime.started = true;
    ESP_LOGI(TAG, "System running — PCUs=%d, pump tasks=%d, tank tasks=%d",
             static_cast<int>(g_runtime.apps.size()),
             static_cast<int>(g_runtime.pump_tasks.size()),
             static_cast<int>(g_runtime.tank_tasks.size()));
}


// ─── Entry point ──────────────────────────────────────────────────────────────

extern "C" void app_main(void) {
    main_tasks_starter();
}