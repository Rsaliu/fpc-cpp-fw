/**
 * @file app_network.cpp
 * @brief Implementation of the Wi-Fi + MQTTS + HTTPS-OTA startup flow.
 *
 * Port of fpc/main/init.c: initialize_ids + swap_in_mqtt_topics +
 * initialize_mqtt_client + initialize_comms_handlers_and_routes, plus the
 * route_callbacks.c HTTPS-OTA helper.
 */

#include "app_network.hpp"

#include "mqtt_conn.hpp"
#include "comms_manager.hpp"
#include "ota_handler.hpp"
#include "utils.hpp"
#include "ota_messages.hpp"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>

namespace fpc::app {
namespace {

const char* TAG = "APP_NETWORK";

// Wi-Fi station credentials (replace before running the MQTT example).
constexpr const char* kWifiSsid = "FPC-TEST";
constexpr const char* kWifiPassword = "test_password";

// MQTTS broker URL/port and the OTA topic templates come from Kconfig
// (CONFIG_FPC_MQTTS_*), mirroring the old fpc/main/Kconfig.projbuild. The
// templates embed this placeholder, swapped for the device MAC at runtime.
constexpr const char* kDeviceIdPlaceholder = "{device_id}";

// NVS partition + namespace holding the TLS certificates (mirrors old fpc init.c).
constexpr const char* kSecretsPartition = "secrets";
constexpr const char* kSecretsNamespace = "certs";
constexpr const char* kKeyCaCert     = "ca_cert";      // server root CA  (MQTT + HTTPS)
constexpr const char* kKeyClientCert = "client_cert";  // device certificate (mutual TLS)
constexpr const char* kKeyClientKey  = "client_key";   // device private key (mutual TLS)
constexpr const char* kKeyHttpsCert  = "https_cert";   // HTTPS/OTA server CA

constexpr int kWifiConnectedBit = BIT0;
constexpr int kWifiFailBit = BIT1;
constexpr int kWifiMaxRetries = 5;

EventGroupHandle_t g_wifi_event_group = nullptr;
int g_wifi_retry_count = 0;

void wifi_event_handler(void*,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data) {
    if (g_wifi_event_group == nullptr) {
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_wifi_retry_count < kWifiMaxRetries) {
            ++g_wifi_retry_count;
            esp_wifi_connect();
            ESP_LOGW(TAG, "Wi-Fi reconnect attempt %d/%d", g_wifi_retry_count, kWifiMaxRetries);
        } else {
            xEventGroupSetBits(g_wifi_event_group, kWifiFailBit);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* got_ip = static_cast<ip_event_got_ip_t*>(event_data);
        g_wifi_retry_count = 0;
        ESP_LOGI(TAG, "Wi-Fi connected, got IP: " IPSTR, IP2STR(&got_ip->ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, kWifiConnectedBit);
    }
}

// ─── TLS certificates from the "secrets" NVS partition ────────────────────────
// Mirrors fpc/main/init.c + route_callbacks.c: certs live in the `secrets`
// partition under namespace `certs`. The init-partition → open → read → close
// dance lives in fpc::utils::read_nvs_blob_from_partition so it is shared by
// MQTTS (ca + client cert/key) and HTTPS/OTA (https_cert).

struct TlsCerts {
    std::string ca_cert;      // server root CA → verify broker (one-way TLS)
    std::string client_cert;  // device cert    → client auth  (mutual TLS)
    std::string client_key;   // device key     → client auth  (mutual TLS)
};

// Read a single PEM blob from the `secrets`/`certs` NVS namespace.
Result<std::string> load_secret(const char* key) {
    return utils::read_nvs_blob_from_partition(kSecretsPartition,
                                               kSecretsNamespace, key);
}

// MQTTS certificates: server CA + device cert/key for mutual TLS.
Result<TlsCerts> load_mqtt_certs() {
    auto ca = load_secret(kKeyCaCert);
    if (ca.is_err())  return Result<TlsCerts>::err(ca.error());

    auto crt = load_secret(kKeyClientCert);
    if (crt.is_err()) return Result<TlsCerts>::err(crt.error());

    auto key = load_secret(kKeyClientKey);
    if (key.is_err()) return Result<TlsCerts>::err(key.error());

    return Result<TlsCerts>::ok(TlsCerts{
        std::move(ca.value()),
        std::move(crt.value()),
        std::move(key.value()),
    });
}

// HTTPS/OTA certificate: the server CA used to verify the firmware host.
Result<std::string> load_https_cert() {
    return load_secret(kKeyHttpsCert);
}

// ─── MQTT client: device-id, topics, connect, subscribe, route ────────────────

// Mirrors the old globals mqtt_topics_t (7 fixed topics).
struct MqttTopics {
    std::string subscribe_topic_job;
    std::string subscribe_topic_rollback;
    std::string publish_checkin_topic;
    std::string publish_ack_topic;
    std::string publish_progress_topic;
    std::string publish_result_topic;
    std::string publish_error_topic;
};

// All state that must outlive the (never-returning) MQTT task. MqttConfig keeps
// non-owning cert pointers, so the backing strings, connection, queue, topics
// and route manager all live here for the task's lifetime. Replaces the old
// C-style `globals` (device_id / mqtt_topics / client handle).
struct MqttRuntimeContext {
    std::string                   device_id;
    TlsCerts                      certs;
    MqttTopics                    topics;
    QueueHandle_t                 queue{nullptr};
    std::unique_ptr<MqttConn>     conn;
    std::unique_ptr<RouteManager> routes;
};

// Port of initialize_ids(): device ID = factory-programmed eFUSE MAC, formatted
// as 12 uppercase hex chars (e.g. "A1B2C3D4E5F6").
Result<std::string> make_device_id() {
    uint8_t mac[6]{};
    esp_err_t err = esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_read_mac(EFUSE_FACTORY) failed: %s", esp_err_to_name(err));
        return Result<std::string>::err(SystemError::Failed);
    }
    char buf[13];
    std::snprintf(buf, sizeof(buf), "%02X%02X%02X%02X%02X%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return Result<std::string>::ok(std::string{buf});
}

// Port of swap_in_mqtt_topics(): fill each topic from its Kconfig template by
// substituting every "{device_id}" with the real device id.
Result<MqttTopics> build_mqtt_topics(std::string_view device_id) {
    MqttTopics t;
    struct Field { const char* tmpl; std::string* out; const char* name; };
    const Field fields[] = {
        { CONFIG_FPC_MQTTS_OTA_SUBSCRIBE_TOPIC_JOB,      &t.subscribe_topic_job,      "job" },
        { CONFIG_FPC_MQTTS_OTA_SUBSCRIBE_TOPIC_ROLLBACK, &t.subscribe_topic_rollback, "rollback" },
        { CONFIG_FPC_MQTTS_OTA_PUBLISH_TOPIC_CHECKIN,    &t.publish_checkin_topic,    "checkin" },
        { CONFIG_FPC_MQTTS_OTA_PUBLISH_TOPIC_ACK,        &t.publish_ack_topic,        "ack" },
        { CONFIG_FPC_MQTTS_OTA_PUBLISH_TOPIC_PROGRESS,   &t.publish_progress_topic,   "progress" },
        { CONFIG_FPC_MQTTS_OTA_PUBLISH_TOPIC_RESULT,     &t.publish_result_topic,     "result" },
        { CONFIG_FPC_MQTTS_OTA_PUBLISH_TOPIC_ERROR,      &t.publish_error_topic,      "error" },
    };
    for (const auto& f : fields) {
        auto r = utils::swap_string_all(f.tmpl, kDeviceIdPlaceholder, device_id);
        if (r.is_err()) {
            ESP_LOGE(TAG, "Failed to build '%s' topic: %s",
                     f.name, to_string(r.error()).data());
            return Result<MqttTopics>::err(r.error());
        }
        *f.out = std::move(r.value());
        ESP_LOGI(TAG, "MQTT topic [%s]: %s", f.name, f.out->c_str());
    }
    return Result<MqttTopics>::ok(std::move(t));
}

// Route handler for the OTA job topic (port route_callback_ota_job_route_callback).
// Parses the standard OTA job JSON via fpc::serialization, then triggers the
// HTTPS OTA download using the `https_cert` from the secrets partition.
void handle_ota_job(const std::string& payload) {
    ESP_LOGI(TAG, "[OTA JOB] payload: %s", payload.c_str());

    auto job_result = serialization::parse_ota_job(payload);
    if (job_result.is_err()) {
        ESP_LOGE(TAG, "[OTA JOB] rejected: %s", to_string(job_result.error()).data());
        return;
    }
    const serialization::OtaJobInfo& job = job_result.value();
    ESP_LOGI(TAG, "[OTA JOB] id=%s version=%s size=%d url=%s",
             job.job_id.c_str(), job.version.c_str(), job.size, job.url.c_str());

    // NOTE: sha256 / signature verification and min_battery_percent gating are
    // not yet enforced here — OtaHandler::download performs the flash write and
    // esp_app image validation. Extend as needed before production.
    if (auto r = run_https_ota(job.url.c_str()); r.is_err()) {
        ESP_LOGE(TAG, "[OTA JOB] HTTPS OTA failed: %s", to_string(r.error()).data());
        return;
    }

    if (job.reboot_after_download) {
        ESP_LOGI(TAG, "[OTA JOB] rebooting into new firmware");
        esp_restart();
    }
}

// Route handler for the OTA rollback topic (logging stub for now).
void handle_ota_rollback(const std::string& payload) {
    auto rb = serialization::parse_ota_rollback(payload);
    if (rb.is_err()) {
        ESP_LOGE(TAG, "[OTA ROLLBACK] rejected: %s", to_string(rb.error()).data());
        return;
    }
    const serialization::OtaRollbackInfo& info = rb.value();
    ESP_LOGI(TAG, "[OTA ROLLBACK] id=%s failed=%s restored=%s reason=%s (handling TBD)",
             info.job_id.c_str(), info.failed_version.c_str(),
             info.restored_version.c_str(), info.reason.c_str());
}

} // namespace

// ─── Public API ───────────────────────────────────────────────────────────────

Result<void> init_wifi_station() {
    if (std::string_view{kWifiSsid}.empty() || std::string_view{kWifiSsid} == "YOUR_WIFI_SSID") {
        ESP_LOGE(TAG, "Set kWifiSsid and kWifiPassword in app_network.cpp before running the MQTT example");
        return Result<void>::err(SystemError::InvalidState);
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return Result<void>::err(SystemError::Failed);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    if (g_wifi_event_group == nullptr) {
        g_wifi_event_group = xEventGroupCreate();
    }
    g_wifi_retry_count = 0;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        nullptr,
                                                        nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        nullptr,
                                                        nullptr));

    wifi_config_t wifi_config{};
    std::snprintf(reinterpret_cast<char*>(wifi_config.sta.ssid),
                  sizeof(wifi_config.sta.ssid),
                  "%s",
                  kWifiSsid);
    std::snprintf(reinterpret_cast<char*>(wifi_config.sta.password),
                  sizeof(wifi_config.sta.password),
                  "%s",
                  kWifiPassword);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    const EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                                 kWifiConnectedBit | kWifiFailBit,
                                                 pdTRUE,
                                                 pdFALSE,
                                                 pdMS_TO_TICKS(30000));
    if ((bits & kWifiConnectedBit) != 0) {
        return Result<void>::ok();
    }

    ESP_LOGE(TAG, "Wi-Fi station connection failed");
    return Result<void>::err(SystemError::Failed);
}

// Example: HTTPS OTA using the `https_cert` from the secrets partition.
// Mirrors fpc route_callbacks.c (get_https_server_cert_from_nvs + downloader).
Result<void> run_https_ota(const char* firmware_url) {
    if (firmware_url == nullptr || firmware_url[0] == '\0') {
        ESP_LOGE(TAG, "run_https_ota: empty firmware URL");
        return Result<void>::err(SystemError::InvalidParameter);
    }

    // The cert string must outlive the download() call below, so keep it local.
    auto cert = load_https_cert();
    if (cert.is_err()) {
        ESP_LOGE(TAG, "Failed to load '%s': %s", kKeyHttpsCert,
                 to_string(cert.error()).data());
        return Result<void>::err(cert.error());
    }

    esp_http_client_config_t http_cfg{};
    http_cfg.url      = firmware_url;
    http_cfg.cert_pem = cert.value().c_str();   // server CA → verify firmware host

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (client == nullptr) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return Result<void>::err(SystemError::Failed);
    }

    // OtaHandler::download opens the client itself. On its error-return paths it
    // already cleans the client up; on success the caller owns the cleanup.
    auto result = OtaHandler::download(client);
    if (result.is_ok()) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        ESP_LOGI(TAG, "OTA image written — call esp_restart() to boot it");
    }
    return result;
}

void mqtt_client_task(void* /*params*/) {
    if (auto wifi_result = init_wifi_station(); wifi_result.is_err()) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", to_string(wifi_result.error()).data());
        vTaskDelete(nullptr);
        return;
    }

    static MqttRuntimeContext ctx;

    // 1. Device ID from the factory eFUSE MAC (port initialize_ids).
    if (auto r = make_device_id(); r.is_ok()) {
        ctx.device_id = std::move(r.value());
    } else {
        ESP_LOGE(TAG, "Device ID init failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "Device ID: %s", ctx.device_id.c_str());

    // 2. Build the OTA topic set from templates (port swap_in_mqtt_topics).
    if (auto r = build_mqtt_topics(ctx.device_id); r.is_ok()) {
        ctx.topics = std::move(r.value());
    } else {
        ESP_LOGE(TAG, "MQTT topic build failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }

    // 3. Message queue shared by MqttConn (producer) and RouteManager (consumer).
    ctx.queue = xQueueCreate(8, sizeof(MqttMessage));
    if (ctx.queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create MQTT message queue");
        vTaskDelete(nullptr);
        return;
    }

    // 4. TLS certs from the `secrets` partition (ca + client cert/key).
    if (auto r = load_mqtt_certs(); r.is_ok()) {
        ctx.certs = std::move(r.value());
    } else {
        ESP_LOGE(TAG, "Failed to load TLS certs from '%s': %s",
                 kSecretsPartition, to_string(r.error()).data());
        vQueueDelete(ctx.queue);
        vTaskDelete(nullptr);
        return;
    }

    // 5. Connect to the broker over mqtts:// with mutual TLS (port initialize_mqtt_client).
    MqttConfig mqtt_cfg{};
    mqtt_cfg.uri             = CONFIG_FPC_MQTTS_BROKER_URL;
    mqtt_cfg.port            = CONFIG_FPC_MQTTS_BROKER_PORT;
    mqtt_cfg.client_id       = ctx.device_id;
    mqtt_cfg.ca_cert_pem     = ctx.certs.ca_cert.c_str();      // verify broker (one-way TLS)
    mqtt_cfg.client_cert_pem = ctx.certs.client_cert.c_str();  // client auth (mutual TLS)
    mqtt_cfg.client_key_pem  = ctx.certs.client_key.c_str();   // client auth (mutual TLS)
    mqtt_cfg.message_queue   = ctx.queue;

    ctx.conn = std::make_unique<MqttConn>(std::move(mqtt_cfg));
    if (auto r = ctx.conn->init(); r.is_err()) {
        ESP_LOGE(TAG, "MQTT init failed: %s", to_string(r.error()).data());
        ctx.conn.reset();
        vQueueDelete(ctx.queue);
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "Connected to broker %s:%d",
             CONFIG_FPC_MQTTS_BROKER_URL, CONFIG_FPC_MQTTS_BROKER_PORT);

    // 6. Subscribe to the OTA job + rollback topics at QoS 1.
    if (auto r = ctx.conn->subscribe(ctx.topics.subscribe_topic_job, 1); r.is_err()) {
        ESP_LOGE(TAG, "Subscribe (job) failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }
    if (auto r = ctx.conn->subscribe(ctx.topics.subscribe_topic_rollback, 1); r.is_err()) {
        ESP_LOGE(TAG, "Subscribe (rollback) failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "Subscribed to job + rollback topics");

    // 7. Wire the RouteManager to dispatch inbound messages to handlers
    //    (port initialize_comms_handlers_and_routes + handle_comms_message_task).
    RouteManagerConfig rm_cfg{};
    rm_cfg.message_queue = ctx.queue;
    ctx.routes = std::make_unique<RouteManager>(std::move(rm_cfg));
    if (auto r = ctx.routes->init(); r.is_err()) {
        ESP_LOGE(TAG, "RouteManager init failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }
    if (auto r = ctx.routes->add_route(ctx.topics.subscribe_topic_job, handle_ota_job);
        r.is_err()) {
        ESP_LOGE(TAG, "add_route(job) failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }
    if (auto r = ctx.routes->add_route(ctx.topics.subscribe_topic_rollback, handle_ota_rollback);
        r.is_err()) {
        ESP_LOGE(TAG, "add_route(rollback) failed: %s", to_string(r.error()).data());
        vTaskDelete(nullptr);
        return;
    }

    // Announce presence on the check-in topic.
    if (auto r = ctx.conn->publish(ctx.topics.publish_checkin_topic,
                                   std::string{"online:"} + ctx.device_id, 1);
        r.is_err()) {
        ESP_LOGW(TAG, "Check-in publish failed: %s", to_string(r.error()).data());
    }

    // 8. Comms loop: dispatch inbound messages, periodic check-in heartbeat.
    TickType_t last_checkin = xTaskGetTickCount();
    while (true) {
        // Blocks up to ~100 ms on the queue, then dispatches by topic.
        (void)ctx.routes->handle_request();

        const TickType_t now = xTaskGetTickCount();
        if (now - last_checkin >= pdMS_TO_TICKS(30000)) {
            if (auto r = ctx.conn->publish(ctx.topics.publish_checkin_topic,
                                           std::string{"heartbeat:"} + ctx.device_id, 1);
                r.is_err()) {
                ESP_LOGE(TAG, "Check-in publish failed: %s", to_string(r.error()).data());
            }
            last_checkin = now;
        }
    }
}

} // namespace fpc::app
