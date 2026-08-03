/**
 * @file frontend_pages_handler.cpp
 * @brief GET  — static file server for the SPIFFS frontend, plus the
 *        OPTIONS  CORS preflight handler.
 *        Port of reference frontend_pages_handler.c + cors_preflight_handler.
 */

#include "webserver_handlers.hpp"

#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "FRONTEND_HANDLER";

namespace {

Result<std::size_t> read_file_content(const char* path, char* buffer, std::size_t buffer_size)
{
    FILE* file = std::fopen(path, "r");
    if (file == nullptr) {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return Result<std::size_t>::err(SystemError::Failed);
    }
    std::fseek(file, 0, SEEK_END);
    const long file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (file_size <= 0 || static_cast<std::size_t>(file_size) >= buffer_size) {
        std::fclose(file);
        ESP_LOGE(TAG, "Invalid file size: %ld", file_size);
        return Result<std::size_t>::err(SystemError::InvalidLength);
    }
    const std::size_t read_size = std::fread(buffer, 1, static_cast<std::size_t>(file_size), file);
    std::fclose(file);
    if (read_size != static_cast<std::size_t>(file_size)) {
        return Result<std::size_t>::err(SystemError::Failed);
    }
    return Result<std::size_t>::ok(read_size);
}

esp_err_t get_page(const char* file_path, httpd_req_t* req)
{
    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    const char* base_path = hc->ctx->base_path;
    if (base_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Base path is null");
        return ESP_FAIL;
    }

    char directory_name[kWebserverMaxPathLen + 1];
    content_directory_name(file_path, directory_name, sizeof(directory_name));

    char full_file_path[520];
    std::snprintf(full_file_path, sizeof(full_file_path), "%s/%s%s",
                  base_path, directory_name, file_path);
    ESP_LOGI(TAG, "Retrieving page: %s", full_file_path);

    char* buf = hc->ctx->scratch;
    auto rd = read_file_content(full_file_path, buf, kScratchBufSize);
    if (rd.is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read page content");
        return ESP_FAIL;
    }
    if (set_content_type_from_file(req, full_file_path) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set content type");
        return ESP_FAIL;
    }

    httpd_resp_set_status(req, HTTPD_200);
    if (httpd_resp_send(req, buf, rd.value()) != ESP_OK) { return ESP_FAIL; }
    ESP_LOGI(TAG, "Page retrieved successfully");
    return ESP_OK;
}

} // namespace

esp_err_t rest_common_get_handler(httpd_req_t* req)
{
    char filepath[32];
    const std::size_t uri_len = std::strlen(req->uri);
    if (uri_len == 0 || req->uri[uri_len - 1] == '/') {
        std::strncpy(filepath, "/home_ui.html", sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    } else {
        std::strncpy(filepath, req->uri, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
    }
    return get_page(filepath, req);
}

esp_err_t cors_preflight_handler(httpd_req_t* req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, nullptr, 0);
}

} // namespace fpc
