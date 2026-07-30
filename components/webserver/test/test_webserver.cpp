#include "unity.h"
#include "webserver.hpp"
#include "esp_netif.h"
#include "esp_event.h"

static fpc::WebserverConfig make_config()
{
    fpc::WebserverConfig cfg;
    cfg.port = 80; cfg.max_connections = 4; cfg.base_path = "/www";
    return cfg;
}

TEST_CASE("Webserver: constructed in Uninitialized state", "[webserver]")
{
    fpc::Webserver ws{make_config()};
    TEST_ASSERT_NULL(ws.get_handle());
}

TEST_CASE("Webserver: start() before init() returns InvalidState", "[webserver][hw]")
{
    fpc::Webserver ws{make_config()};
    TEST_ASSERT_TRUE(ws.start().is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)ws.start().error());
}

TEST_CASE("Webserver: stop() before start() returns InvalidState", "[webserver][hw]")
{
    fpc::Webserver ws{make_config()};
    (void)ws.init();
    TEST_ASSERT_TRUE(ws.stop().is_err());
    (void)ws.deinit();
}

TEST_CASE("Webserver: deinit() before init() returns InvalidState", "[webserver]")
{
    fpc::Webserver ws{make_config()};
    TEST_ASSERT_TRUE(ws.deinit().is_err());
}

TEST_CASE("Webserver: double init() returns InvalidState", "[webserver][hw]")
{
    fpc::Webserver ws{make_config()};
    (void)ws.init();
    auto r = ws.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
    (void)ws.deinit();
}

TEST_CASE("Webserver: add_route(nullptr) returns NullParameter", "[webserver]")
{
    fpc::Webserver ws{make_config()};
    (void)ws.init();
    TEST_ASSERT_TRUE(ws.add_route(nullptr).is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)ws.add_route(nullptr).error());
    (void)ws.deinit();
}

TEST_CASE("Webserver: add_route when not running returns InvalidState", "[webserver]")
{
    httpd_uri_t dummy{"/api", HTTP_GET, nullptr, nullptr};
    fpc::Webserver ws{make_config()};
    (void)ws.init();
    auto r = ws.add_route(&dummy);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
    (void)ws.deinit();
}

TEST_CASE("Webserver: full lifecycle init start stop deinit", "[webserver][hw]")
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    
    fpc::Webserver ws{make_config()};
    TEST_ASSERT_TRUE(ws.init().is_ok());
    TEST_ASSERT_TRUE(ws.start().is_ok());
    TEST_ASSERT_NOT_NULL(ws.get_handle());
    TEST_ASSERT_TRUE(ws.stop().is_ok());
    TEST_ASSERT_NULL(ws.get_handle());
    TEST_ASSERT_TRUE(ws.deinit().is_ok());
}

TEST_CASE("Webserver: get_context returns non-null after init", "[webserver]")
{
    fpc::Webserver ws{make_config()};
    (void)ws.init();
    TEST_ASSERT_NOT_NULL(ws.get_context());
    (void)ws.deinit();
}
