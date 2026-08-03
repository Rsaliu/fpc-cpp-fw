/**
 * @file webserver_handlers.hpp
 * @brief HTTP route handlers (login/register/logout/reset/config/static) and
 *        the auth gate, plus the route-registration factory.
 *
 * Handlers keep the ESP-IDF `esp_err_t(httpd_req_t*)` signature and obtain their
 * collaborators through a `HandlerContext*` stored in `req->user_ctx`.
 *
 * C++17 / fpc-cpp port of the reference *_handler.c files and web_server_setup.h.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "handler_context.hpp"
#include "session_manager.hpp"
#include "webserver.hpp"   // IWebServer, WebserverSetupFn

namespace fpc {

// ─── Auth gate ────────────────────────────────────────────────────────────────

/// Validate the request's SID cookie against the session table.
/// On success writes the session into @p session and returns ESP_OK;
/// otherwise sends a 401 and returns ESP_FAIL.
esp_err_t auth_gate(httpd_req_t* req, Session** session);

// ─── Route handlers ───────────────────────────────────────────────────────────

esp_err_t login_handler(httpd_req_t* req);           ///< POST /login
esp_err_t register_handler(httpd_req_t* req);        ///< POST /register
esp_err_t logout_handler(httpd_req_t* req);          ///< GET  /logout  (auth)
esp_err_t reset_handler(httpd_req_t* req);           ///< GET  /reset
esp_err_t get_config_handler(httpd_req_t* req);      ///< GET  /config  (auth)
esp_err_t set_config_handler(httpd_req_t* req);      ///< POST /config  (auth)
esp_err_t rest_common_get_handler(httpd_req_t* req); ///< GET  /*  (static files)
esp_err_t cors_preflight_handler(httpd_req_t* req);  ///< OPTIONS /*

// ─── Route registration factory ───────────────────────────────────────────────

/// Build a WebserverSetupFn that registers every route, binding @p handler_ctx
/// as each handler's user_ctx. @p handler_ctx must outlive the running server.
WebserverSetupFn make_webserver_routes(HandlerContext& handler_ctx);

} // namespace fpc
