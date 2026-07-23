#include "unity.h"
#include "comms_manager.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <cstring>

static fpc::RouteManagerConfig make_config(QueueHandle_t q)
{
    fpc::RouteManagerConfig cfg; cfg.message_queue = q; return cfg;
}

TEST_CASE("RouteManager: init with null queue returns NullParameter", "[comms_manager]")
{
    fpc::RouteManager rm{make_config(nullptr)};
    TEST_ASSERT_TRUE(rm.init().is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)rm.init().error());
}

TEST_CASE("RouteManager: init with valid queue succeeds", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)};
    TEST_ASSERT_TRUE(rm.init().is_ok());
    TEST_ASSERT_TRUE(rm.is_initialized());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: add_route before init returns InvalidState", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)};
    auto r = rm.add_route("topic", [](const std::string&) {});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: add_route empty topic returns InvalidParameter", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)}; TEST_ASSERT_TRUE(rm.init().is_ok());
    auto r = rm.add_route("", [](const std::string&) {});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: add_route null handler returns InvalidParameter", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)}; TEST_ASSERT_TRUE(rm.init().is_ok());
    auto r = rm.add_route("t", nullptr);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: add_route increments route_count", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)}; TEST_ASSERT_TRUE(rm.init().is_ok());
    TEST_ASSERT_TRUE(rm.add_route("a", [](const std::string&) {}).is_ok());
    TEST_ASSERT_TRUE(rm.add_route("b", [](const std::string&) {}).is_ok());
    TEST_ASSERT_EQUAL_INT(2, (int)rm.route_count());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: handle_request before init returns InvalidState", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)};
    TEST_ASSERT_TRUE(rm.handle_request().is_err());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: handle_request dispatches correct handler", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)}; TEST_ASSERT_TRUE(rm.init().is_ok());

    std::string received;
    TEST_ASSERT_TRUE(rm.add_route("pump/cmd", [&received](const std::string& d) { received = d; }).is_ok());

    fpc::MqttMessage msg{};
    strncpy(msg.topic, "pump/cmd", sizeof(msg.topic) - 1);
    strncpy(msg.data,  "start",    sizeof(msg.data) - 1);
    xQueueSendToBack(q, &msg, portMAX_DELAY);

    TEST_ASSERT_TRUE(rm.handle_request().is_ok());
    TEST_ASSERT_EQUAL_STRING("start", received.c_str());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: unmatched topic returns InvalidResponse", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)}; TEST_ASSERT_TRUE(rm.init().is_ok());
    TEST_ASSERT_TRUE(rm.add_route("pump/cmd", [](const std::string&) {}).is_ok());

    fpc::MqttMessage msg{};
    strncpy(msg.topic, "tank/cmd", sizeof(msg.topic) - 1);
    xQueueSendToBack(q, &msg, portMAX_DELAY);

    auto r = rm.handle_request();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidResponse, (int)r.error());
    vQueueDelete(q);
}

TEST_CASE("RouteManager: empty queue returns Failed", "[comms_manager]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::RouteManager rm{make_config(q)}; TEST_ASSERT_TRUE(rm.init().is_ok());
    TEST_ASSERT_TRUE(rm.handle_request().is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::Failed, (int)rm.handle_request().error());
    vQueueDelete(q);
}
