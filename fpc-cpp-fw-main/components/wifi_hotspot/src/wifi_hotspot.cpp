#include "wifi_hotspot.hpp"
#include <cstring>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_mac.h"

static const char* TAG = "WIFI_HOTSPOT";

namespace fpc {

static bool map_auth(WifiHotspotAuthMode in, wifi_auth_mode_t& out)
{
    switch (in) {
    case WifiHotspotAuthMode::Open: out = WIFI_AUTH_OPEN;     return true;
    case WifiHotspotAuthMode::WPA2: out = WIFI_AUTH_WPA2_PSK; return true;
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
    case WifiHotspotAuthMode::WPA3: out = WIFI_AUTH_WPA3_PSK; return true;
#endif
    default: return false;
    }
}

static void wifi_event_handler(void*, esp_event_base_t, int32_t event_id, void* data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        auto* ev = static_cast<wifi_event_ap_staconnected_t*>(data);
        ESP_LOGI(TAG, "Station " MACSTR " joined AID=%d", MAC2STR(ev->mac), ev->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        auto* ev = static_cast<wifi_event_ap_stadisconnected_t*>(data);
        ESP_LOGI(TAG, "Station " MACSTR " left AID=%d", MAC2STR(ev->mac), ev->aid);
    }
}

WifiHotspot::WifiHotspot(WifiHotspotConfig config) : config_{std::move(config)} {}

WifiHotspot::~WifiHotspot()
{
    if (state_ != State::Uninitialized) (void)deinit();
}

Result<void> WifiHotspot::init()
{
    if (state_ != State::Uninitialized)
        return Result<void>::err(SystemError::InvalidState);

    if (config_.ssid.empty() || config_.ssid.size() > 31)
        return Result<void>::err(SystemError::InvalidParameter);
    if (config_.auth_mode != WifiHotspotAuthMode::Open) {
        if (config_.password.size() < 8 || config_.password.size() > 63)
            return Result<void>::err(SystemError::InvalidParameter);
    }
    if (config_.channel < 1 || config_.channel > 13)
        return Result<void>::err(SystemError::InvalidParameter);
    if (config_.max_connections < 1 || config_.max_connections > 10)
        return Result<void>::err(SystemError::InvalidParameter);

    wifi_auth_mode_t esp_auth;
    if (!map_auth(config_.auth_mode, esp_auth))
        return Result<void>::err(SystemError::InvalidParameter);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return Result<void>::err(SystemError::Failed);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, nullptr));

    state_ = State::Initialized;
    ESP_LOGI(TAG, "Hotspot initialized");
    return Result<void>::ok();
}

Result<void> WifiHotspot::deinit()
{
    if (state_ == State::Uninitialized)
        return Result<void>::err(SystemError::InvalidState);

    if (state_ == State::Running) {
        ESP_ERROR_CHECK(esp_wifi_stop());
        state_ = State::Initialized;
    }
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_event_loop_delete_default());

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (netif) esp_netif_destroy(netif);

    state_ = State::Uninitialized;
    return Result<void>::ok();
}

Result<void> WifiHotspot::on()
{
    if (state_ == State::Uninitialized) return Result<void>::err(SystemError::InvalidState);
    if (state_ == State::Running)       return Result<void>::err(SystemError::InvalidState);

    wifi_auth_mode_t esp_auth;
    if (!map_auth(config_.auth_mode, esp_auth))
        return Result<void>::err(SystemError::InvalidParameter);

    wifi_config_t wifi_cfg{};
    wifi_cfg.ap.ssid_len       = static_cast<uint8_t>(config_.ssid.size());
    wifi_cfg.ap.channel        = config_.channel;
    wifi_cfg.ap.max_connection = config_.max_connections;
    wifi_cfg.ap.authmode       = esp_auth;
    wifi_cfg.ap.pmf_cfg.required = true;
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
    wifi_cfg.ap.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
#endif
    std::strncpy(reinterpret_cast<char*>(wifi_cfg.ap.ssid),
                 config_.ssid.c_str(), sizeof(wifi_cfg.ap.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi_cfg.ap.password),
                 config_.password.c_str(), sizeof(wifi_cfg.ap.password) - 1);
    if (config_.password.empty()) wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    state_ = State::Running;
    ESP_LOGI(TAG, "Hotspot started SSID:'%s'", config_.ssid.c_str());
    return Result<void>::ok();
}

Result<void> WifiHotspot::off()
{
    if (state_ != State::Running) return Result<void>::err(SystemError::InvalidState);
    ESP_ERROR_CHECK(esp_wifi_stop());
    state_ = State::Initialized;
    return Result<void>::ok();
}

bool WifiHotspot::is_running()     const noexcept { return state_ == State::Running; }
bool WifiHotspot::is_initialized() const noexcept { return state_ != State::Uninitialized; }

} // namespace fpc
