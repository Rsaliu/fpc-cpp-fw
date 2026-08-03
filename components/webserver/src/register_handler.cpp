/**
 * @file register_handler.cpp
 * @brief POST /register — one-time user registration.
 *        Port of reference register_handler.c.
 */

#include "webserver_handlers.hpp"

#include <string>
#include <nlohmann/json.hpp>
#include "esp_log.h"
#include "credential_store.hpp"
#include "webserver_utils.hpp"

namespace fpc {

static const char* TAG = "REGISTER_HANDLER";

esp_err_t register_handler(httpd_req_t* req)
{
    if (req == nullptr) { return ESP_ERR_INVALID_ARG; }
    inject_cors(req);

    auto* hc = static_cast<HandlerContext*>(req->user_ctx);
    if (hc == nullptr || hc->ctx == nullptr || hc->creds == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "handler context missing");
        return ESP_FAIL;
    }
    char* buf = hc->ctx->scratch;

    auto reg = hc->creds->is_registered();
    if (reg.is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to check user registration status", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    if (reg.value()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("User already registered", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    if (retrieve_request_body(req, buf, kScratchBufSize).is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to retrieve request body", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    auto root = nlohmann::json::parse(buf, nullptr, false);
    if (root.is_discarded() || !root.contains("username") ||
        !root.contains("password1") || !root.contains("password2") ||
        !root["username"].is_string() || !root["password1"].is_string() ||
        !root["password2"].is_string()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid JSON format", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    const std::string username  = root["username"].get<std::string>();
    const std::string password1 = root["password1"].get<std::string>();
    const std::string password2 = root["password2"].get<std::string>();

    if (username.empty() || username.size() > static_cast<std::size_t>(CONFIG_MAX_USERNAME_LENGTH)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid username length", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    if (password1.empty() || password1.size() > static_cast<std::size_t>(CONFIG_MAX_PASSWORD_LENGTH)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid password length", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    if (password2.empty() || password2.size() > static_cast<std::size_t>(CONFIG_MAX_PASSWORD_LENGTH)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Invalid password length", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    if (password1 != password2) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
            make_json_message("Passwords do not match", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    if (hc->creds->set(username, password1).is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to store credentials", buf, kScratchBufSize));
        return ESP_FAIL;
    }
    if (hc->creds->set_registered().is_err()) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
            make_json_message("Failed to set user registration flag", buf, kScratchBufSize));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "User %s registered successfully", username.c_str());
    httpd_resp_sendstr(req,
        make_json_message("Registration successful", buf, kScratchBufSize));
    return ESP_OK;
}

} // namespace fpc
