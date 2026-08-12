/**
 * @file route_registry.cpp
 * @brief RouteRegistry implementation — ordered, introspectable route table.
 */

#include "route_registry.hpp"

#include <cstring>
#include "esp_log.h"

namespace fpc {

static const char* TAG = "ROUTE_REGISTRY";

bool RouteRegistry::is_wildcard(const RouteDef& r) noexcept
{
    return (r.uri != nullptr) && (std::strcmp(r.uri, "/*") == 0);
}

bool RouteRegistry::has_duplicate(const RouteDef& r) const noexcept
{
    for (const auto& existing : routes_) {
        if ((existing.method == r.method) && (std::strcmp(existing.uri, r.uri) == 0)) {
            return true;
        }
    }
    return false;
}

Result<void> RouteRegistry::add(const RouteDef& route)
{
    if ((route.uri == nullptr) || (route.handler == nullptr)) {
        ESP_LOGE(TAG, "add(): null uri or handler");
        return Result<void>::err(SystemError::NullParameter);
    }
    if (has_duplicate(route)) {
        ESP_LOGE(TAG, "add(): duplicate route %s (method %d)", route.uri,
                 static_cast<int>(route.method));
        return Result<void>::err(SystemError::InvalidParameter);
    }
    routes_.push_back(route);
    return Result<void>::ok();
}

Result<void> RouteRegistry::register_all(IWebServer& server) const
{
    // Two passes: specific routes first, wildcard ("/*") routes last.
    for (int pass = 0; pass < 2; ++pass) {
        const bool want_wildcard = (pass == 1);
        for (const auto& r : routes_) {
            if (is_wildcard(r) != want_wildcard) {
                continue;
            }
            httpd_uri_t uri{};
            uri.uri      = r.uri;
            uri.method   = r.method;
            uri.handler  = r.handler;
            uri.user_ctx = r.user_ctx;
            auto res = server.add_route(&uri);
            if (res.is_err()) {
                ESP_LOGE(TAG, "Failed to register route %s", r.uri);
                return res;
            }
        }
    }
    ESP_LOGI(TAG, "Registered %u route(s)", static_cast<unsigned>(routes_.size()));
    return Result<void>::ok();
}

Result<void> RouteRegistry::unregister_all(IWebServer& server) const
{
    Result<void> first_error = Result<void>::ok();
    for (auto it = routes_.rbegin(); it != routes_.rend(); ++it) {
        auto res = server.remove_route(it->uri, it->method);
        if (res.is_err() && first_error.is_ok()) {
            ESP_LOGE(TAG, "Failed to unregister route %s", it->uri);
            first_error = res;
        }
    }
    return first_error;
}

} // namespace fpc
