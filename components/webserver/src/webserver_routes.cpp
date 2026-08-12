/**
 * @file webserver_routes.cpp
 * @brief make_webserver_routes — WebserverSetupFn that registers every route
 *        through a RouteRegistry. Port of reference web_server_setup.h.
 */

#include "webserver_handlers.hpp"
#include "route_registry.hpp"

#include "esp_log.h"

namespace fpc {

static const char* TAG = "WEB_ROUTES";

WebserverSetupFn make_webserver_routes(HandlerContext& handler_ctx)
{
    HandlerContext* ctx = &handler_ctx;

    return [ctx](IWebServer& server) -> Result<void> {
        RouteRegistry registry;

        struct Entry {
            const char*    uri;
            httpd_method_t method;
            esp_err_t    (*handler)(httpd_req_t*);
            const char*    description;
            bool           auth_required;
        };

        // Wildcard routes may appear anywhere; RouteRegistry registers them last.
        static const Entry kRoutes[] = {
            {"/*",        HTTP_OPTIONS, cors_preflight_handler,  "CORS preflight",        false},
            {"/reset",    HTTP_GET,     reset_handler,           "Factory-reset creds",   false},
            {"/register", HTTP_POST,    register_handler,        "Register credentials",  false},
            {"/logout",   HTTP_GET,     logout_handler,          "Close session",         true},
            {"/login",    HTTP_POST,    login_handler,           "Open session",          false},
            {"/config",   HTTP_GET,     get_config_handler,      "Read configuration",    true},
            {"/config",   HTTP_POST,    set_config_handler,      "Write configuration",   true},
            {"/*",        HTTP_GET,     rest_common_get_handler, "Static frontend files", false},
        };

        for (const auto& e : kRoutes) {
            RouteDef def{};
            def.uri           = e.uri;
            def.method        = e.method;
            def.handler       = e.handler;
            def.description   = e.description;
            def.auth_required = e.auth_required;
            def.user_ctx      = ctx;
            auto res = registry.add(def);
            if (res.is_err()) {
                ESP_LOGE(TAG, "Failed to add route %s to registry", e.uri);
                return res;
            }
        }

        auto reg = registry.register_all(server);
        if (reg.is_err()) {
            return reg;
        }
        ESP_LOGI(TAG, "All webserver routes registered");
        return Result<void>::ok();
    };
}

} // namespace fpc
