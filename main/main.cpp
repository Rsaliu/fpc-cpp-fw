/**
 * @file example.cpp
 * @brief Standalone example: complete app_main that brings up NVS, netif,
 *        the Wi-Fi hotspot, and the fpc webserver with all routes registered.
 *
 * NOT compiled into the component — paste directly into main/main.cpp
 * (or add this file to your app's SRCS and remove your own app_main).
 *
 * After boot, connect to the "ESP32-WebServer" AP (password123) and browse
 * to http://192.168.4.1/. The REST API is documented in api/openapi.yaml.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "webserver.hpp"
#include "webserver_handlers.hpp"   // make_webserver_routes
#include "handler_context.hpp"
#include "session_manager.hpp"
#include "credential_store.hpp"
#include "wifi_hotspot.hpp"

static const char* TAG = "WEB_EXAMPLE";

namespace {

// Long-lived collaborators — must outlive the running server, hence static.
fpc::Webserver*       g_server{nullptr};
fpc::SessionManager   g_sessions;
fpc::CredentialStore  g_creds;
fpc::HandlerContext   g_handler_ctx;
fpc::WifiHotspot*     g_hotspot{nullptr};

} // namespace

/**
 * @brief Construct, initialise and start the webserver, then register every
 *        HTTP route (login/logout/register/reset/config and static files).
 *
 * @return Result<void>::ok() on success, or the first error encountered.
 */
fpc::Result<void> example_start_webserver()
{
    // 1. Configure the server.
    fpc::WebserverConfig cfg;
    cfg.port                = 80;
    cfg.max_connections     = 4;
    cfg.mdns_instance       = "FPC-WebServer";
    cfg.mdns_hostname       = "fpc-webserver";
    cfg.base_path           = "/spiffs";           // SPIFFS mount point
    cfg.web_mount_point     = "/spiffs";
    cfg.web_partition_label = "spiffs";
    cfg.config_file_path    = "config.json";

    // 2. Create the server (static so it survives after this function returns).
    static fpc::Webserver server{cfg};
    g_server = &server;

    // 3. Initialise the credential store (NVS-backed).
    auto res = g_creds.init();
    if (res.is_err()) {
        ESP_LOGE(TAG, "Credential store init failed: %s", fpc::to_string(res.error()).data());
        return res;
    }

    // 4. Init + start the HTTP server.
    res = server.init();
    if (res.is_err()) {
        ESP_LOGE(TAG, "Webserver init failed: %s", fpc::to_string(res.error()).data());
        return res;
    }
    res = server.start();
    if (res.is_err()) {
        ESP_LOGE(TAG, "Webserver start failed: %s", fpc::to_string(res.error()).data());
        (void)server.deinit();
        return res;
    }

    // 5. Bundle the collaborators every handler needs.
    g_handler_ctx.ctx      = server.get_context();
    g_handler_ctx.sessions = &g_sessions;
    g_handler_ctx.creds    = &g_creds;

    // 6. Build the setup function and register all routes.
    fpc::WebserverSetupFn setup = fpc::make_webserver_routes(g_handler_ctx);
    res = setup(server);
    if (res.is_err()) {
        ESP_LOGE(TAG, "Route registration failed: %s", fpc::to_string(res.error()).data());
        (void)server.stop();
        (void)server.deinit();
        return res;
    }

    ESP_LOGI(TAG, "Webserver up — API spec: api/openapi.yaml in the repo");
    return fpc::Result<void>::ok();
}

/**
 * @brief Gracefully stop and tear down the webserver started above.
 */
fpc::Result<void> example_stop_webserver()
{
    if (g_server == nullptr) {
        return fpc::Result<void>::err(fpc::SystemError::InvalidState);
    }
    auto res = g_server->stop();
    if (res.is_err()) { return res; }
    res = g_server->deinit();
    g_sessions.clear();
    (void)g_creds.deinit();
    return res;
}

/**
 * @brief Start the Wi-Fi soft-AP so clients can reach the webserver.
 */
static fpc::Result<void> example_start_hotspot()
{
    fpc::WifiHotspotConfig ap_cfg{};
    ap_cfg.ssid            = "ESP32-WebServer5";
    ap_cfg.password        = "password123";
    ap_cfg.channel         = 1;
    ap_cfg.max_connections = 5;
    ap_cfg.auth_mode       = fpc::WifiHotspotAuthMode::WPA2;

    static fpc::WifiHotspot hotspot{std::move(ap_cfg)};
    g_hotspot = &hotspot;

    auto res = hotspot.init();
    if (res.is_err()) {
        ESP_LOGE(TAG, "WifiHotspot init failed: %s", fpc::to_string(res.error()).data());
        return res;
    }
    res = hotspot.on();
    if (res.is_err()) {
        ESP_LOGE(TAG, "WifiHotspot on failed: %s", fpc::to_string(res.error()).data());
        (void)hotspot.deinit();
        return res;
    }
    return fpc::Result<void>::ok();
}

// ─── Entry point ────────────────────────────────────────────────────────────────

extern "C" void app_main(void)
{
    // 1. Bring up the soft-AP. WifiHotspot::init() internally initialises
    //    NVS, esp_netif and the default event loop — do NOT create them here,
    //    or the second esp_event_loop_create_default() call will abort with
    //    ESP_ERR_INVALID_STATE.
    if (example_start_hotspot().is_err()) {
        ESP_LOGE(TAG, "Hotspot failed to start — aborting");
        return;
    }

    // 2. Start the webserver with all routes registered.
    if (example_start_webserver().is_err()) {
        ESP_LOGE(TAG, "Webserver failed to start");
        if (g_hotspot != nullptr) {
            (void)g_hotspot->off();
            (void)g_hotspot->deinit();
        }
        return;
    }

    ESP_LOGI(TAG, "Ready — join 'ESP32-WebServer' and open http://192.168.4.1/");

    // 5. Keep app_main alive (server runs in the httpd task).
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
