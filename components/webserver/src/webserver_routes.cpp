/**
 * @file webserver_routes.cpp
 * @brief make_webserver_routes — WebserverSetupFn that registers every route.
 *        Port of reference web_server_setup.h::setup_web_handlers.
 */

#include "webserver_handlers.hpp"

#include "esp_log.h"

namespace fpc {

static const char* TAG = "WEB_ROUTES";

WebserverSetupFn make_webserver_routes(HandlerContext& handler_ctx)
{
    HandlerContext* ctx = &handler_ctx;

    return [ctx](IWebServer& server) -> Result<void> {
        struct Route {
            const char*             uri;
            httpd_method_t          method;
            esp_err_t (*handler)(httpd_req_t*);
        };

        // Order matters: the "/*" catch-all must be registered last.
        static const Route kRoutes[] = {
            {"/*",        HTTP_OPTIONS, cors_preflight_handler},
            {"/reset",    HTTP_GET,     reset_handler},
            {"/register", HTTP_POST,    register_handler},
            {"/logout",   HTTP_GET,     logout_handler},
            {"/login",    HTTP_POST,    login_handler},
            {"/config",   HTTP_GET,     get_config_handler},
            {"/config",   HTTP_POST,    set_config_handler},
            {"/*",        HTTP_GET,     rest_common_get_handler},
        };

        for (const auto& r : kRoutes) {
            httpd_uri_t uri{};
            uri.uri      = r.uri;
            uri.method   = r.method;
            uri.handler  = r.handler;
            uri.user_ctx = ctx;
            auto res = server.add_route(&uri);
            if (res.is_err()) {
                ESP_LOGE(TAG, "Failed to register route %s", r.uri);
                return res;
            }
        }
        ESP_LOGI(TAG, "All webserver routes registered");
        return Result<void>::ok();
    };
}

} // namespace fpc
