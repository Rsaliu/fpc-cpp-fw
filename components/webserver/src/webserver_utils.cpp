/**
 * @file webserver_utils.cpp
 * @brief Implementation of webserver helper functions (port of webserver_utils.c).
 */

#include "handler_context.hpp"
#include <webserver.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include "esp_log.h"
#include "esp_netif.h"

namespace fpc {

static const char* TAG = "WEB_SERVER_UTILS";

namespace {


/// Case-insensitive check that @p filename ends with @p ext.
bool has_extension(std::string_view filename, std::string_view ext)
{
    if (filename.size() < ext.size()) { return false; }
    return strncasecmp(filename.data() + (filename.size() - ext.size()),
                       ext.data(), ext.size()) == 0;
}

} // namespace

Result<void> retrieve_request_body(httpd_req_t* req, char* buffer, std::size_t buffer_size)
{
    if (req == nullptr || buffer == nullptr || buffer_size == 0) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    const int total_len = req->content_len;
    if (static_cast<std::size_t>(total_len) > buffer_size - 1) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return Result<void>::err(SystemError::BufferOverflow);
    }

    int cur_len = 0;
    while (cur_len < total_len) {
        const int received = httpd_req_recv(req, buffer + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                "Failed to read request body");
            return Result<void>::err(SystemError::Failed);
        }
        cur_len += received;
    }
    buffer[total_len] = '\0';
    return Result<void>::ok();
}

void inject_cors(httpd_req_t* req)
{

    #ifdef SWAGGER_CORS
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    #else
        auto* hc = static_cast<HandlerContext*>(req->user_ctx);
        if (hc == nullptr || hc->ctx == nullptr) {
            ESP_LOGE(TAG, "Handler context is missing");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
            return;
        }
        auto* server_url = hc->ctx->server_url;
        if(!server_url) {
            ESP_LOGE(TAG, "Server URL is null");
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Server URL is null");
            return;
        }
        ESP_LOGI(TAG, "the server URL for CORS is: %s",server_url);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", server_url);
    #endif
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials", "true");
}

Result<void> parse_cookie(std::string_view cookie_hdr, std::string_view name,
                          char* out, std::size_t out_len)
{
    char buf[128];
    const std::size_t n = std::min(cookie_hdr.size(), sizeof(buf) - 1);
    std::memcpy(buf, cookie_hdr.data(), n);
    buf[n] = '\0';

    char* saveptr = nullptr;
    char* token = strtok_r(buf, ";", &saveptr);
    while (token != nullptr) {
        while (*token == ' ') { ++token; }
        const std::size_t name_len = name.size();
        if (std::strncmp(token, name.data(), name_len) == 0 && token[name_len] == '=') {
            const char* val = token + name_len + 1;
            std::strncpy(out, val, out_len - 1);
            out[out_len - 1] = '\0';
            return Result<void>::ok();
        }
        token = strtok_r(nullptr, ";", &saveptr);
    }
    return Result<void>::err(SystemError::Failed);
}

esp_err_t set_content_type_from_file(httpd_req_t* req, std::string_view filepath)
{
    const char* type = "text/plain";
    if      (has_extension(filepath, ".html")) { type = "text/html"; }
    else if (has_extension(filepath, ".js"))   { type = "application/javascript"; }
    else if (has_extension(filepath, ".css"))  { type = "text/css"; }
    else if (has_extension(filepath, ".png"))  { type = "image/png"; }
    else if (has_extension(filepath, ".ico"))  { type = "image/x-icon"; }
    else if (has_extension(filepath, ".svg"))  { type = "text/xml"; }
    return httpd_resp_set_type(req, type);
}

void content_directory_name(std::string_view filepath, char* buff, std::size_t buff_size)
{
    const char* dir = "";
    if      (has_extension(filepath, ".html")) { dir = "html"; }
    else if (has_extension(filepath, ".js"))   { dir = "js"; }
    else if (has_extension(filepath, ".css"))  { dir = "css"; }
    else if (has_extension(filepath, ".png"))  { dir = "img"; }
    else if (has_extension(filepath, ".ico"))  { dir = "img"; }
    else if (has_extension(filepath, ".svg"))  { dir = "img"; }
    std::strncpy(buff, dir, buff_size - 1);
    buff[buff_size - 1] = '\0';
}

const char* make_json_message(std::string_view message, char* buffer, std::size_t buffer_size)
{
    if (buffer != nullptr) {
        std::snprintf(buffer, buffer_size, "{ \"message\": \"%.*s\" }",
                      static_cast<int>(message.size()), message.data());
    }
    return buffer;
}

} // namespace fpc
