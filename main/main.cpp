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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "setup_config_button.hpp"

using namespace fpc;

static const char *TAG = "APP_MAIN";
static constexpr const char *kConfigPath = "/spiffs/config.json";

// Embedded fallback config (mirrors fpc/main/main.c valid_json).
// In production replace with ConfigManager::read_file("/spiffs/config.json").
static const char *kDefaultJson =
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
    "          \"height_mm\": 200.0 * 10, \"full_level_mm\": 1800, \"low_level_mm\": 200"
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

struct RuntimeContext
{
    std::vector<Application> apps;
    std::vector<std::unique_ptr<PumpMonitorTask>> pump_tasks;
    std::vector<std::unique_ptr<TankMonitorTask>> tank_tasks;
    bool started{false};
};

static RuntimeContext g_runtime;

struct WebRuntimeContext
{
    std::unique_ptr<WifiHotspot> hotspot;
    std::unique_ptr<WebserverTask> web_task;
    bool started{false};
};

static WebRuntimeContext g_web_runtime;

static void set_cors_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static std::string resolve_config_path(const WebserverContext *ctx)
{
    if (ctx == nullptr)
    {
        return std::string{kConfigPath};
    }

    std::string cfg = ctx->config_file_path;
    if (!cfg.empty() && cfg.front() == '/')
    {
        return cfg;
    }

    std::string base = ctx->base_path;
    if (base.empty())
    {
        return cfg.empty() ? std::string{kConfigPath} : cfg;
    }

    if (!base.empty() && base.back() == '/')
    {
        return base + cfg;
    }
    return base + "/" + cfg;
}

static esp_err_t options_handler(httpd_req_t *req)
{
    set_cors_headers(req);
    return httpd_resp_send(req, nullptr, 0);
}

static esp_err_t health_handler(httpd_req_t *req)
{
    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
}

static esp_err_t get_config_handler(httpd_req_t *req)
{
    auto *ctx = static_cast<WebserverContext *>(req->user_ctx);
    const auto path = resolve_config_path(ctx);

    auto read_result = ConfigManager::read_file(path.c_str());
    if (read_result.is_err())
    {
        set_cors_headers(req);
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "{\"error\":\"config_not_found\"}");
    }

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req,
                           read_result.value().c_str(),
                           static_cast<ssize_t>(read_result.value().size()));
}

static esp_err_t set_config_handler(httpd_req_t *req)
{
    auto *ctx = static_cast<WebserverContext *>(req->user_ctx);
    if (ctx == nullptr)
    {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"missing_context\"}");
    }

    if (req->content_len <= 0 || req->content_len >= static_cast<int>(kScratchBufSize))
    {
        set_cors_headers(req);
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_sendstr(req, "{\"error\":\"payload_too_large\"}");
    }

    int total = 0;
    while (total < req->content_len)
    {
        int received = httpd_req_recv(req,
                                      ctx->scratch + total,
                                      static_cast<size_t>(req->content_len - total));
        if (received <= 0)
        {
            set_cors_headers(req);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_sendstr(req, "{\"error\":\"read_failed\"}");
        }
        total += received;
    }
    ctx->scratch[total] = '\0';

    auto parse_result = ConfigManager::parse(std::string_view{ctx->scratch, static_cast<size_t>(total)});
    if (parse_result.is_err())
    {
        set_cors_headers(req);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"error\":\"invalid_json\"}");
    }

    const auto path = resolve_config_path(ctx);
    FILE *fp = std::fopen(path.c_str(), "w");
    if (fp == nullptr)
    {
        set_cors_headers(req);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"write_open_failed\"}");
    }

    const size_t written = std::fwrite(ctx->scratch, 1, static_cast<size_t>(total), fp);
    std::fclose(fp);
    if (written != static_cast<size_t>(total))
    {
        set_cors_headers(req);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "{\"error\":\"write_failed\"}");
    }

    set_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"status\":\"saved\"}");
}

static Result<AppSetupConfig> load_config()
{
    auto file_result = ConfigManager::read_file(kConfigPath);
    if (file_result.is_ok())
    {
        ESP_LOGI(TAG, "Loaded config from %s", kConfigPath);
        return ConfigManager::parse(file_result.value());
    }

    ESP_LOGW(TAG, "Config file not found at %s, using embedded example JSON", kConfigPath);
    return ConfigManager::parse(kDefaultJson);
}

static Result<void> setup_system_from_config(const AppSetupConfig &app_cfg)
{
    if (app_cfg.pump_control_units.empty())
    {
        ESP_LOGE(TAG, "No pump_control_units in config");
        return Result<void>::err(SystemError::InvalidParameter);
    }

    g_runtime.apps.clear();
    g_runtime.apps.reserve(app_cfg.pump_control_units.size());

    for (const auto &pcu_cfg : app_cfg.pump_control_units)
    {
        auto app_result = Factory::create_from_config(pcu_cfg);
        if (app_result.is_err())
        {
            return Result<void>::err(app_result.error());
        }
        g_runtime.apps.push_back(std::move(app_result.value()));
    }

    return Result<void>::ok();
}

static Result<void> setup_tasks_from_config()
{
    g_runtime.pump_tasks.clear();
    g_runtime.tank_tasks.clear();

    for (size_t i = 0; i < g_runtime.apps.size(); ++i)
    {
        auto &app = g_runtime.apps[i];

        for (size_t j = 0; j < app.pump_monitors.size(); ++j)
        {
            PumpMonitorTaskConfig pm_cfg;
            pm_cfg.id = static_cast<int32_t>(j + 1);
            pm_cfg.monitor = app.pump_monitors[j].get();
            pm_cfg.check_interval_ms = 100;

            auto task = std::make_unique<PumpMonitorTask>(pm_cfg);
            auto start_result = task->start();
            if (start_result.is_err())
            {
                return Result<void>::err(start_result.error());
            }
            g_runtime.pump_tasks.push_back(std::move(task));
        }

        if (!app.tank_monitors.empty())
        {
            TankMonitorTaskConfig tm_cfg;
            tm_cfg.id = static_cast<int32_t>(i + 1);
            tm_cfg.check_interval_ms = 1000;
            tm_cfg.monitors.reserve(app.tank_monitors.size());

            for (auto &tm : app.tank_monitors)
            {
                tm_cfg.monitors.push_back(tm.get());
            }

            auto task = std::make_unique<TankMonitorTask>(std::move(tm_cfg));
            auto start_result = task->start();
            if (start_result.is_err())
            {
                return Result<void>::err(start_result.error());
            }
            g_runtime.tank_tasks.push_back(std::move(task));
        }
    }

    return Result<void>::ok();
}

// ─── Normal operation ─────────────────────────────────────────────────────────

static void main_tasks_starter()
{
    if (g_runtime.started)
    {
        ESP_LOGW(TAG, "Main tasks already started");
        return;
    }

    // 1. Parse config (filesystem first, fallback example JSON)
    auto cfg_result = load_config();
    if (cfg_result.is_err())
    {
        ESP_LOGE(TAG, "Config parse failed: %s",
                 to_string(cfg_result.error()).data());
        return;
    }

    // 2. Setup full system object graph from config
    if (auto r = setup_system_from_config(cfg_result.value()); r.is_err())
    {
        ESP_LOGE(TAG, "System setup failed: %s", to_string(r.error()).data());
        return;
    }

    // 3. Setup and start tasks from config
    if (auto r = setup_tasks_from_config(); r.is_err())
    {
        ESP_LOGE(TAG, "Task setup failed: %s", to_string(r.error()).data());
        return;
    }

    g_runtime.started = true;
    ESP_LOGI(TAG, "System running — PCUs=%d, pump tasks=%d, tank tasks=%d",
             static_cast<int>(g_runtime.apps.size()),
             static_cast<int>(g_runtime.pump_tasks.size()),
             static_cast<int>(g_runtime.tank_tasks.size()));
}

// ─── Config (webserver) mode — placeholder ────────────────────────────────────

static void webserver_task_fn(void * /*params*/)
{
    if (g_web_runtime.started)
    {
        ESP_LOGW(TAG, "Webserver mode already running");
        vTaskDelete(nullptr);
        return;
    }

    WifiHotspotConfig ap_cfg{};
    ap_cfg.ssid = "ESP32-WebServer";
    ap_cfg.password = "password123";
    ap_cfg.channel = 1;
    ap_cfg.max_connections = 5;
    ap_cfg.auth_mode = WifiHotspotAuthMode::WPA2;

    g_web_runtime.hotspot = std::make_unique<WifiHotspot>(std::move(ap_cfg));

    if (auto r = g_web_runtime.hotspot->init(); r.is_err())
    {
        ESP_LOGE(TAG, "WifiHotspot init failed: %s", to_string(r.error()).data());
        g_web_runtime.hotspot.reset();
        vTaskDelete(nullptr);
        return;
    }
    if (auto r = g_web_runtime.hotspot->on(); r.is_err())
    {
        ESP_LOGE(TAG, "WifiHotspot on failed: %s", to_string(r.error()).data());
        (void)g_web_runtime.hotspot->deinit();
        g_web_runtime.hotspot.reset();
        vTaskDelete(nullptr);
        return;
    }

    WebserverTaskConfig ws_cfg{};
    ws_cfg.webserver_config.port = 80;
    ws_cfg.webserver_config.max_connections = 4;
    ws_cfg.webserver_config.mdns_instance = "FPC-WebServer";
    ws_cfg.webserver_config.mdns_hostname = "fpc-webserver";
    ws_cfg.webserver_config.base_path = "/spiffs";
    ws_cfg.webserver_config.web_mount_point = "/spiffs";
    ws_cfg.webserver_config.web_partition_label = "spiffs";
    ws_cfg.webserver_config.config_file_path = "config.json";
    ws_cfg.setup_fn = [](IWebServer &server) -> Result<void>
    {
        auto *ctx = server.get_context();

        static httpd_uri_t health_uri{};
        health_uri.uri = "/health";
        health_uri.method = HTTP_GET;
        health_uri.handler = health_handler;
        health_uri.user_ctx = ctx;
        if (auto r = server.add_route(&health_uri); r.is_err())
        {
            return r;
        }

        static httpd_uri_t get_config_uri{};
        get_config_uri.uri = "/config";
        get_config_uri.method = HTTP_GET;
        get_config_uri.handler = get_config_handler;
        get_config_uri.user_ctx = ctx;
        if (auto r = server.add_route(&get_config_uri); r.is_err())
        {
            return r;
        }

        static httpd_uri_t set_config_uri{};
        set_config_uri.uri = "/config";
        set_config_uri.method = HTTP_POST;
        set_config_uri.handler = set_config_handler;
        set_config_uri.user_ctx = ctx;
        if (auto r = server.add_route(&set_config_uri); r.is_err())
        {
            return r;
        }

        static httpd_uri_t options_uri{};
        options_uri.uri = "/*";
        options_uri.method = HTTP_OPTIONS;
        options_uri.handler = options_handler;
        options_uri.user_ctx = ctx;
        if (auto r = server.add_route(&options_uri); r.is_err())
        {
            return r;
        }

        return Result<void>::ok();
    };

    g_web_runtime.web_task = std::make_unique<WebserverTask>(std::move(ws_cfg));
    if (auto r = g_web_runtime.web_task->start(); r.is_err())
    {
        ESP_LOGE(TAG, "WebserverTask start failed: %s", to_string(r.error()).data());
        (void)g_web_runtime.hotspot->off();
        (void)g_web_runtime.hotspot->deinit();
        g_web_runtime.web_task.reset();
        g_web_runtime.hotspot.reset();
        vTaskDelete(nullptr);
        return;
    }

    g_web_runtime.started = true;
    ESP_LOGI(TAG, "Webserver mode running");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void webserver_task_starter()
{
    xTaskCreate(webserver_task_fn, "webserver", 8192, nullptr, 5, nullptr);
}

// ─── Entry point ──────────────────────────────────────────────────────────────

// extern "C" void app_main(void) {
// DeviceModeConfig dm_cfg{
//     .button_pin   = GPIO_NUM_16,
//     .main_task_cb = main_tasks_starter,
//     .webserver_cb = webserver_task_starter,
// };

// static DeviceMode dm{dm_cfg};

// if (auto r = dm.init(); r.is_err()) {
//     ESP_LOGE(TAG, "DeviceMode init failed: %s", to_string(r.error()).data());
//     return;
// }

// if (auto r = dm.handle_event(); r.is_err()) {
//     ESP_LOGE(TAG, "DeviceMode handle_event failed: %s", to_string(r.error()).data());
// }

void main_task (){
    for(;;){
        ESP_LOGI(TAG, "Main task running...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    } 
}

void webserver_task(){
    for(;;){
        ESP_LOGI(TAG, "Webserver task running...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    } 
}

extern "C" void app_main(void)
{
    SetupConfigButtonConfig button_config{
        .button_pin = GPIO_NUM_16,
        .main_task_cb = main_task,
        .webserver_cb = webserver_task,
    };

    SetupConfigButton button(button_config);

    auto result = button.init();

    if (result.is_err())
    {
        ESP_LOGE(TAG, "Button initialization failed: %s",
                 to_string(result.error()).data());
        return;
    }

    ESP_LOGI(TAG, "Button initialized");

    int previous_state = gpio_get_level(GPIO_NUM_16);

    while (true)
    {
        int state = gpio_get_level(GPIO_NUM_16);

        ESP_LOGI(TAG, "Button state = %d", state);

        vTaskDelay(pdMS_TO_TICKS(500));

    }
}
