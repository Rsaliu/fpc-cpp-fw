/**
 * @file auth.cpp
 * @brief auth_gate — cookie → session validation for protected routes.
 *        Port of reference auth.c.
 */

#include "webserver_handlers.hpp"

#include "esp_log.h"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "AUTH_HANDLER";

esp_err_t auth_gate(httpd_req_t* req, Session** session)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->sessions == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }

    char cookie_hdr[128];
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie_hdr, sizeof(cookie_hdr)) != ESP_OK) {
        ESP_LOGE(TAG, "Cookie header not found");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Cookie header not found");
        return ESP_FAIL;
    }

    char sid[kSessionTokenLen + 1];
    if (parse_cookie(cookie_hdr, "SID", sid, sizeof(sid)).is_err()) {
        ESP_LOGE(TAG, "Session ID not found in Cookie header");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Session ID not found in Cookie header");
        return ESP_FAIL;
    }

    Session* s = hc->sessions->find_by_token(sid);
    if (s == nullptr) {
        ESP_LOGI(TAG, "Session is NULL");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Unauthorized access");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Session username: %s", s->username.data());
    *session = s;
    return ESP_OK;
}

} // namespace fpc
