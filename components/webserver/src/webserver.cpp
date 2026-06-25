#include "webserver.hpp"
#include <cstring>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"

static const char* TAG = "WEBSERVER";
static constexpr int RESERVED_SOCKETS = 3;
static constexpr int MAX_URI_HANDLERS = 20;

namespace fpc {

Webserver::Webserver(WebserverConfig config)
    : config_{std::move(config)}
{
    std::memset(&context_, 0, sizeof(context_));
}

Webserver::~Webserver()
{
    if (state_ == State::Running) (void)stop();
    if (state_ == State::Initialized || state_ == State::Stopped) (void)deinit();
}

Result<void> Webserver::init()
{
    if (state_ != State::Uninitialized)
        return Result<void>::err(SystemError::InvalidState);

    std::strncpy(context_.base_path,
                 config_.base_path.c_str(), kWebserverMaxPathLen);
    std::strncpy(context_.config_file_path,
                 config_.config_file_path.c_str(), kWebserverMaxPathLen);

    if (!config_.web_partition_label.empty() && !config_.web_mount_point.empty()) {
        esp_vfs_spiffs_conf_t spiffs_cfg{};
        spiffs_cfg.base_path              = config_.web_mount_point.c_str();
        spiffs_cfg.partition_label        = config_.web_partition_label.c_str();
        spiffs_cfg.max_files              = 10;
        spiffs_cfg.format_if_mount_failed = true;
        if (esp_vfs_spiffs_register(&spiffs_cfg) != ESP_OK) {
            ESP_LOGE(TAG, "SPIFFS mount failed");
            return Result<void>::err(SystemError::Failed);
        }
    }

    state_ = State::Initialized;
    ESP_LOGI(TAG, "Webserver initialized (port=%d)", config_.port);
    return Result<void>::ok();
}

Result<void> Webserver::start()
{
    if (state_ != State::Initialized && state_ != State::Stopped)
        return Result<void>::err(SystemError::InvalidState);

    httpd_config_t httpd_cfg   = HTTPD_DEFAULT_CONFIG();
    httpd_cfg.server_port      = static_cast<uint16_t>(config_.port);
    httpd_cfg.max_open_sockets = static_cast<uint16_t>(config_.max_connections + RESERVED_SOCKETS);
    httpd_cfg.max_uri_handlers = static_cast<uint16_t>(MAX_URI_HANDLERS);
    httpd_cfg.uri_match_fn     = httpd_uri_match_wildcard;

    if (httpd_start(&server_, &httpd_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return Result<void>::err(SystemError::Failed);
    }

    state_ = State::Running;
    ESP_LOGI(TAG, "Webserver started on port %d", config_.port);
    return Result<void>::ok();
}

Result<void> Webserver::stop()
{
    if (state_ != State::Running)
        return Result<void>::err(SystemError::InvalidState);
    if (server_ != nullptr) { httpd_stop(server_); server_ = nullptr; }
    state_ = State::Stopped;
    return Result<void>::ok();
}

Result<void> Webserver::deinit()
{
    if (state_ == State::Uninitialized)
        return Result<void>::err(SystemError::InvalidState);
    if (state_ == State::Running) (void)stop();
    if (!config_.web_partition_label.empty())
        esp_vfs_spiffs_unregister(config_.web_partition_label.c_str());
    state_ = State::Uninitialized;
    return Result<void>::ok();
}

Result<void> Webserver::add_route(httpd_uri_t* uri)
{
    if (uri == nullptr) return Result<void>::err(SystemError::NullParameter);
    if (state_ != State::Running) return Result<void>::err(SystemError::InvalidState);
    if (httpd_register_uri_handler(server_, uri) != ESP_OK)
        return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

Result<void> Webserver::remove_route(const char* uri, httpd_method_t method)
{
    if (uri == nullptr) return Result<void>::err(SystemError::NullParameter);
    if (state_ != State::Running) return Result<void>::err(SystemError::InvalidState);
    if (httpd_unregister_uri_handler(server_, uri, method) != ESP_OK)
        return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

httpd_handle_t    Webserver::get_handle()  const noexcept { return server_; }
WebserverContext* Webserver::get_context() const noexcept
    { return const_cast<WebserverContext*>(&context_); }

} // namespace fpc
