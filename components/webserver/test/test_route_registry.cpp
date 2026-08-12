/**
 * @file test_route_registry.cpp
 * @brief Unity tests for RouteRegistry and the OpenAPI spec handler.
 */

#include "unity.h"
#include "route_registry.hpp"

#include <string>
#include <vector>
#include <cstring>

namespace {

esp_err_t dummy_handler(httpd_req_t*) { return ESP_OK; }
esp_err_t dummy_handler2(httpd_req_t*) { return ESP_OK; }

/// Mock IWebServer that records add_route order and can inject failures.
class MockWebServer final : public fpc::IWebServer {
public:
    fpc::Result<void> init()   override { return fpc::Result<void>::ok(); }
    fpc::Result<void> start()  override { return fpc::Result<void>::ok(); }
    fpc::Result<void> stop()   override { return fpc::Result<void>::ok(); }
    fpc::Result<void> deinit() override { return fpc::Result<void>::ok(); }

    fpc::Result<void> add_route(httpd_uri_t* uri) override
    {
        if (fail_on_add) { return fpc::Result<void>::err(fpc::SystemError::OperationFailed); }
        added.emplace_back(uri->uri);
        return fpc::Result<void>::ok();
    }

    fpc::Result<void> remove_route(const char* uri, httpd_method_t) override
    {
        removed.emplace_back(uri);
        return fpc::Result<void>::ok();
    }

    [[nodiscard]] httpd_handle_t         get_handle()  const noexcept override { return nullptr; }
    [[nodiscard]] fpc::WebserverContext* get_context() const noexcept override { return nullptr; }

    bool fail_on_add{false};
    std::vector<std::string> added;
    std::vector<std::string> removed;
};

fpc::RouteDef make_route(const char* uri, httpd_method_t method,
                         esp_err_t (*handler)(httpd_req_t*) = dummy_handler)
{
    fpc::RouteDef r{};
    r.uri = uri; r.method = method; r.handler = handler;
    return r;
}

} // namespace

TEST_CASE("RouteRegistry: add with null uri fails", "[route_registry]")
{
    fpc::RouteRegistry reg;
    auto r = reg.add(make_route(nullptr, HTTP_GET));
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}

TEST_CASE("RouteRegistry: add with null handler fails", "[route_registry]")
{
    fpc::RouteRegistry reg;
    auto r = reg.add(make_route("/a", HTTP_GET, nullptr));
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}

TEST_CASE("RouteRegistry: duplicate (uri, method) rejected", "[route_registry]")
{
    fpc::RouteRegistry reg;
    TEST_ASSERT_TRUE(reg.add(make_route("/a", HTTP_GET)).is_ok());
    auto r = reg.add(make_route("/a", HTTP_GET, dummy_handler2));
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
    TEST_ASSERT_EQUAL_UINT(1U, (unsigned)reg.size());
}

TEST_CASE("RouteRegistry: same uri different method allowed", "[route_registry]")
{
    fpc::RouteRegistry reg;
    TEST_ASSERT_TRUE(reg.add(make_route("/a", HTTP_GET)).is_ok());
    TEST_ASSERT_TRUE(reg.add(make_route("/a", HTTP_POST)).is_ok());
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)reg.size());
}

TEST_CASE("RouteRegistry: wildcard routes registered last", "[route_registry]")
{
    fpc::RouteRegistry reg;
    TEST_ASSERT_TRUE(reg.add(make_route("/*", HTTP_GET)).is_ok());
    TEST_ASSERT_TRUE(reg.add(make_route("/login", HTTP_POST)).is_ok());
    TEST_ASSERT_TRUE(reg.add(make_route("/*", HTTP_OPTIONS)).is_ok());
    TEST_ASSERT_TRUE(reg.add(make_route("/config", HTTP_GET)).is_ok());

    MockWebServer server;
    TEST_ASSERT_TRUE(reg.register_all(server).is_ok());
    TEST_ASSERT_EQUAL_UINT(4U, (unsigned)server.added.size());
    TEST_ASSERT_EQUAL_STRING("/login",  server.added[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/config", server.added[1].c_str());
    TEST_ASSERT_EQUAL_STRING("/*",      server.added[2].c_str());
    TEST_ASSERT_EQUAL_STRING("/*",      server.added[3].c_str());
}

TEST_CASE("RouteRegistry: registration failure propagates", "[route_registry]")
{
    fpc::RouteRegistry reg;
    TEST_ASSERT_TRUE(reg.add(make_route("/a", HTTP_GET)).is_ok());

    MockWebServer server;
    server.fail_on_add = true;
    auto r = reg.register_all(server);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::OperationFailed, (int)r.error());
}

TEST_CASE("RouteRegistry: unregister_all removes in reverse order", "[route_registry]")
{
    fpc::RouteRegistry reg;
    TEST_ASSERT_TRUE(reg.add(make_route("/a", HTTP_GET)).is_ok());
    TEST_ASSERT_TRUE(reg.add(make_route("/b", HTTP_GET)).is_ok());

    MockWebServer server;
    TEST_ASSERT_TRUE(reg.unregister_all(server).is_ok());
    TEST_ASSERT_EQUAL_UINT(2U, (unsigned)server.removed.size());
    TEST_ASSERT_EQUAL_STRING("/b", server.removed[0].c_str());
    TEST_ASSERT_EQUAL_STRING("/a", server.removed[1].c_str());
}
