/**
 * @file config_handler.cpp
 * @brief GET/POST /config — read and persist the JSON configuration file.
 *        Port of reference config_handler.c. Both routes are auth-gated.
 */

#include "webserver_handlers.hpp"

#include <cstdio>
#include <string>
#include <nlohmann/json.hpp>
#include "esp_log.h"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "CONFIG_HANDLER";

namespace {

Result<void> save_config_to_file(const nlohmann::json& config, const char* path)
{
    const std::string out = config.dump();
    FILE* file = std::fopen(path, "w");
    if (file == nullptr) {
        ESP_LOGE(TAG, "Failed to open config file for writing");
        return Result<void>::err(SystemError::OperationFailed);
    }
    std::fwrite(out.data(), 1, out.size(), file);
    std::fclose(file);
    return Result<void>::ok();
}

} // namespace

esp_err_t set_config_handler(httpd_req_t* req)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }
    inject_cors(req);

    if (auth_gate(req, &sessio) != ESP_OK) { 
        return ESP_FAIL; 
    }

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    char* buf = hc->ctx->scratch;

    if (retrieve_request_body(req, buf, kScratchBufSize).is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to retrieve request body", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    [[maybe_unused]] Session* session = nullptr;
    if (auth_gate(req, &session) != ESP_OK) { return ESP_FAIL; }

    auto root = nlohmann::json::parse(buf, nullptr, false);
    if (root.is_discarded()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid JSON format", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    const char* base_path        = hc->ctx->base_path;
    const char* config_file_path = hc->ctx->config_file_path;
    if (base_path[0] == '\0' || config_file_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Base path or config file path is null", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    char full_config_path[520];
    const int path_len = std::snprintf(full_config_path, sizeof(full_config_path), "%s/%s",
                                       base_path, config_file_path);
    if (path_len < 0 || static_cast<std::size_t>(path_len) >= sizeof(full_config_path)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Config path too long", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Storing configuration to file: %s", full_config_path);

    if (save_config_to_file(root, full_config_path).is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to save configuration", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, HTTPD_200);
    if (httpd_resp_sendstr(req,
            make_json_message("Configuration saved successfully", buf, kScratchBufSize)) != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "config saved successfully");
    return ESP_OK;
}

esp_err_t get_config_handler(httpd_req_t* req)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }
    inject_cors(req);

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    char* buf = hc->ctx->scratch;

    [[maybe_unused]] Session* session = nullptr;
    if (auth_gate(req, &session) != ESP_OK) { return ESP_FAIL; }

    const char* base_path        = hc->ctx->base_path;
    const char* config_file_path = hc->ctx->config_file_path;
    if (base_path[0] == '\0' || config_file_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Base path or config file path is null", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    char full_config_path[520];
    const int path_len = std::snprintf(full_config_path, sizeof(full_config_path), "%s/%s",
                                       base_path, config_file_path);
    if (path_len < 0 || static_cast<std::size_t>(path_len) >= sizeof(full_config_path)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Config path too long", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    FILE* file = std::fopen(full_config_path, "r");
    if (file == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to open config file", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    std::fseek(file, 0, SEEK_END);
    const long file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (file_size <= 0 || static_cast<std::size_t>(file_size) >= kScratchBufSize) {
        std::fclose(file);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid file size", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    const std::size_t read_size = std::fread(buf, 1, static_cast<std::size_t>(file_size), file);
    std::fclose(file);
    if (read_size != static_cast<std::size_t>(file_size)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to read config file", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    buf[read_size] = '\0';

    httpd_resp_set_status(req, HTTPD_200);
    if (httpd_resp_sendstr(req, buf) != ESP_OK) { return ESP_FAIL; }
    ESP_LOGI(TAG, "Configuration retrieved successfully");
    return ESP_OK;
}

} // namespace fpc
