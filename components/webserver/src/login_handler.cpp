/**
 * @file login_handler.cpp
 * @brief POST /login — validate credentials and open a session.
 *        Port of reference login_handler.c.
 */

#include "webserver_handlers.hpp"

#include <string_view>
#include <nlohmann/json.hpp>
#include "esp_log.h"
#include "credential_store.hpp"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "LOGIN_HANDLER";

esp_err_t login_handler(httpd_req_t* req)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }
    inject_cors(req);

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr || hc->sessions == nullptr || hc->creds == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    char* buf = hc->ctx->scratch;

    if (retrieve_request_body(req, buf, kScratchBufSize).is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to retrieve request body", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    auto root = nlohmann::json::parse(buf, nullptr, false);
    if (root.is_discarded() || !root.contains("username") || !root.contains("password") ||
        !root["username"].is_string() || !root["password"].is_string()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid JSON format", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    const std::string username = root["username"].get<std::string>();
    const std::string password = root["password"].get<std::string>();

    if (username.empty() || username.size() > static_cast<std::size_t>(CONFIG_MAX_USERNAME_LENGTH)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("username has invalid length", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    if (password.empty() || password.size() > static_cast<std::size_t>(CONFIG_MAX_PASSWORD_LENGTH)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("password has invalid length", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    if (!hc->creds->check(username, password)) {
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED,
            make_json_message("Invalid username or password", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    if (hc->sessions->find_by_username(username) != nullptr) {
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_sendstr(req,
            make_json_message("User already logged in", buf, kScratchBufSize));
        ESP_LOGI(TAG, "User already logged in");
        return ESP_OK;
    }

    Session* s = hc->sessions->create(username);
    if (s == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to create session", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    hc->sessions->set_cookie(req, s);

    httpd_resp_set_status(req, HTTPD_200);
    ESP_LOGI(TAG, "User %s logged in successfully", username.c_str());
    if (httpd_resp_sendstr(req,
            make_json_message("Login successful", buf, kScratchBufSize)) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

} // namespace fpc
