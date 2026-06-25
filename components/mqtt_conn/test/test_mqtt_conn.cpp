#include "unity.h"
#include "mqtt_conn.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static fpc::MqttConfig make_config(QueueHandle_t q = nullptr)
{
    fpc::MqttConfig cfg;
    cfg.uri = "mqtts://test.broker:8883"; cfg.port = 8883;
    cfg.client_id = "test_client"; cfg.message_queue = q;
    return cfg;
}

TEST_CASE("MqttConn: not connected after construction", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    TEST_ASSERT_FALSE(c.is_connected());
    vQueueDelete(q);
}

TEST_CASE("MqttConn: init with null queue returns NullParameter", "[mqtt_conn]")
{
    fpc::MqttConn c{make_config(nullptr)};
    auto r = c.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}

TEST_CASE("MqttConn: subscribe when not connected returns InvalidState", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    TEST_ASSERT_TRUE(c.subscribe("t").is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)c.subscribe("t").error());
    vQueueDelete(q);
}

TEST_CASE("MqttConn: unsubscribe when not connected returns InvalidState", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    TEST_ASSERT_TRUE(c.unsubscribe("t").is_err());
    vQueueDelete(q);
}

TEST_CASE("MqttConn: publish when not connected returns InvalidState", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    TEST_ASSERT_TRUE(c.publish("t", "data").is_err());
    vQueueDelete(q);
}

TEST_CASE("MqttConn: subscribe with empty topic returns InvalidParameter", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    auto r = c.subscribe("");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
    vQueueDelete(q);
}

TEST_CASE("MqttConn: publish empty topic returns InvalidParameter", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    auto r = c.publish("", "data");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
    vQueueDelete(q);
}

TEST_CASE("MqttConn: deinit when not connected returns InvalidState", "[mqtt_conn]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::MqttMessage));
    fpc::MqttConn c{make_config(q)};
    TEST_ASSERT_TRUE(c.deinit().is_err());
    vQueueDelete(q);
}
