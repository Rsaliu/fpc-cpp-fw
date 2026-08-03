/**
 * @file reset_handler.cpp
 * @brief GET /reset — factory reset: clear sessions, credentials, config file.
 *        Port of reference reset_handler.c.
 */

#include "webserver_handlers.hpp"

#include <cstdio>
#include "esp_log.h"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "RESET_HANDLER";

esp_err_t reset_handler(httpd_req_t* req)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }
    inject_cors(req);

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr || hc->sessions == nullptr || hc->creds == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    char* buf = hc->ctx->scratch;

    hc->sessions->clear();

    if (hc->creds->clear().is_err()) {
        ESP_LOGE(TAG, "Failed to clear credential store");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to clear credential store", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Credential store cleared successfully");

    const char* base_path        = hc->ctx->base_path;
    const char* config_file_path = hc->ctx->config_file_path;
    if (base_path[0] == '\0' || config_file_path[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Base path or config file path is null", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    char full_config_path[520];
    std::snprintf(full_config_path, sizeof(full_config_path), "%s/%s",
                  base_path, config_file_path);
    std::remove(full_config_path);
    ESP_LOGI(TAG, "Configuration file cleared: %s", full_config_path);

    httpd_resp_set_status(req, HTTPD_200);
    return httpd_resp_sendstr(req,
        make_json_message("Reset successful", buf, kScratchBufSize));
}

} // namespace fpc
