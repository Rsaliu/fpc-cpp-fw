/**
 * @file route_registry.hpp
 * @brief Declarative HTTP route table with ordered registration.
 *
 * A RouteRegistry collects RouteDef entries and registers/unregisters them
 * against an IWebServer. Wildcard (catch-all) routes are always registered last,
 * regardless of insertion order, so specific routes take precedence.
 * The route table can be introspected (e.g. by the OpenAPI generator).
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <vector>
#include "common.hpp"
#include "webserver.hpp"   // IWebServer
#include "esp_http_server.h"

namespace fpc {

/// Declarative description of a single HTTP route.
struct RouteDef {
    const char*    uri{nullptr};              ///< URI pattern (e.g. "/login" or the catch-all wildcard).
    httpd_method_t method{HTTP_GET};          ///< HTTP method.
    esp_err_t    (*handler)(httpd_req_t*){nullptr}; ///< ESP-IDF handler function.
    const char*    description{""};           ///< Human-readable summary (for docs).
    bool           auth_required{false};      ///< True if the route sits behind the auth gate.
    void*          user_ctx{nullptr};         ///< Passed as req->user_ctx.
};

/**
 * @brief Ordered, introspectable collection of HTTP routes.
 *
 * Duplicate (uri, method) pairs are rejected. Wildcard (catch-all) routes are
 * deferred so that register_all() always installs them after every
 * specific route, mirroring esp_http_server matching requirements.
 */
class RouteRegistry {
public:
    /// Add a route definition. Fails with InvalidParameter on duplicates or
    /// NullParameter when uri/handler are null. Returns *this-style chaining
    /// via the Result-free fluent add() overload is intentionally avoided so
    /// failures are never silently dropped.
    Result<void> add(const RouteDef& route);

    /// Register every stored route on @p server (wildcards last).
    /// Stops and propagates the first failure.
    Result<void> register_all(IWebServer& server) const;

    /// Unregister every stored route from @p server (reverse order).
    /// Attempts all removals; returns the first error encountered, if any.
    Result<void> unregister_all(IWebServer& server) const;

    /// Introspection access for documentation generators.
    [[nodiscard]] const std::vector<RouteDef>& routes() const noexcept { return routes_; }

    /// Number of stored routes.
    [[nodiscard]] std::size_t size() const noexcept { return routes_.size(); }

private:
    [[nodiscard]] static bool is_wildcard(const RouteDef& r) noexcept;
    [[nodiscard]] bool has_duplicate(const RouteDef& r) const noexcept;

    std::vector<RouteDef> routes_;
};

} // namespace fpc
