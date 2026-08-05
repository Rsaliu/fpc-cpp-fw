/**
 * @file logout_handler.cpp
 * @brief GET /logout — end the caller's session (auth-gated).
 *        Port of reference logout_handler.c.
 */

#include "webserver_handlers.hpp"

#include "esp_log.h"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "LOGOUT_HANDLER";

esp_err_t logout_handler(httpd_req_t* req)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }
    inject_cors(req);

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr || hc->sessions == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    char* buf = hc->ctx->scratch;

    Session* session = nullptr;
    if (auth_gate(req, &session) != ESP_OK) { return ESP_FAIL; }
    if (session == nullptr) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED,
            make_json_message("Unauthorized access", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    hc->sessions->remove(session);
    httpd_resp_set_hdr(req, "Set-Cookie", "SID=; Max-Age=0; Path=/; HttpOnly");

    httpd_resp_set_status(req, HTTPD_200);
    if (httpd_resp_sendstr(req,
            make_json_message("Logout successful", buf, kScratchBufSize)) != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "User logged out successfully");
    return ESP_OK;
}

} // namespace fpc
