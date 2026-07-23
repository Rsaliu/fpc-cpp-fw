#include "mqtt_conn.hpp"
#include <cinttypes>
#include <cstring>
#include <algorithm>
#include "mqtt_client.h"
#include "esp_log.h"

[[maybe_unused]] static const char* TAG = "MQTT_CONN";

namespace fpc {

void MqttConn::event_handler(void* handler_args, esp_event_base_t,
                               int32_t event_id, void* event_data)
{
    MqttConn* self = static_cast<MqttConn*>(handler_args);
    auto* ev = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        xEventGroupSetBits(self->event_group_, kConnectedBit);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT disconnected");
        self->connected_ = false;
        break;
    case MQTT_EVENT_DATA: {
        MqttMessage msg{};
        std::size_t tc = std::min((std::size_t)ev->topic_len, sizeof(msg.topic) - 1);
        std::size_t dc = std::min((std::size_t)ev->data_len,  sizeof(msg.data) - 1);
        std::memcpy(msg.topic, ev->topic, tc);
        std::memcpy(msg.data,  ev->data,  dc);
        if (self->config_.message_queue)
            xQueueSendToBack(self->config_.message_queue, &msg,
                             pdMS_TO_TICKS(kMaxQueueWaitMs));
        break;
    }
    case MQTT_EVENT_ERROR:
        xEventGroupSetBits(self->event_group_, kFailBit);
        break;
    default: break;
    }
}

MqttConn::MqttConn(MqttConfig config) : config_{std::move(config)}
{
    event_group_ = xEventGroupCreate();
}

MqttConn::~MqttConn()
{
    if (connected_) (void)deinit();
    if (client_ != nullptr) {
        esp_mqtt_client_destroy(static_cast<esp_mqtt_client_handle_t>(client_));
        client_ = nullptr;
    }
    if (event_group_) { vEventGroupDelete(event_group_); event_group_ = nullptr; }
}

Result<void> MqttConn::init()
{
    if (config_.message_queue == nullptr)
        return Result<void>::err(SystemError::NullParameter);

    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.broker.address.uri              = config_.uri.c_str();
    mqtt_cfg.broker.address.port             = static_cast<uint32_t>(config_.port);
    mqtt_cfg.broker.verification.certificate = config_.ca_cert_pem;
    mqtt_cfg.credentials.client_id           = config_.client_id.c_str();
    mqtt_cfg.credentials.authentication.certificate = config_.client_cert_pem;
    mqtt_cfg.credentials.authentication.key         = config_.client_key_pem;

    auto* esp_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!esp_client) return Result<void>::err(SystemError::Failed);

    esp_mqtt_client_register_event(esp_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), event_handler, this);

    if (esp_mqtt_client_start(esp_client) != ESP_OK) {
        esp_mqtt_client_destroy(esp_client);
        return Result<void>::err(SystemError::Failed);
    }
    client_ = esp_client;

    EventBits_t bits = xEventGroupWaitBits(event_group_,
        kConnectedBit | kFailBit, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & kConnectedBit) { connected_ = true; return Result<void>::ok(); }

    esp_mqtt_client_stop(esp_client);
    esp_mqtt_client_destroy(esp_client);
    client_ = nullptr;
    return Result<void>::err(SystemError::Failed);
}

Result<void> MqttConn::deinit()
{
    if (!connected_) return Result<void>::err(SystemError::InvalidState);
    auto* esp_client = static_cast<esp_mqtt_client_handle_t>(client_);
    if (esp_mqtt_client_stop(esp_client) != ESP_OK)
        return Result<void>::err(SystemError::Failed);
    connected_ = false;
    return Result<void>::ok();
}

Result<void> MqttConn::subscribe(const std::string& topic, int qos)
{
    if (topic.empty())  return Result<void>::err(SystemError::InvalidParameter);
    if (!connected_)    return Result<void>::err(SystemError::InvalidState);
    if (esp_mqtt_client_subscribe(
            static_cast<esp_mqtt_client_handle_t>(client_),
            topic.c_str(), qos) < 0)
        return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

Result<void> MqttConn::unsubscribe(const std::string& topic)
{
    if (topic.empty()) return Result<void>::err(SystemError::InvalidParameter);
    if (!connected_)   return Result<void>::err(SystemError::InvalidState);
    if (esp_mqtt_client_unsubscribe(
            static_cast<esp_mqtt_client_handle_t>(client_),
            topic.c_str()) < 0)
        return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

Result<void> MqttConn::publish(const std::string& topic, const std::string& data,
                                 int qos, bool retain)
{
    if (topic.empty() || data.empty())
        return Result<void>::err(SystemError::InvalidParameter);
    if (!connected_) return Result<void>::err(SystemError::InvalidState);
    if (esp_mqtt_client_publish(
            static_cast<esp_mqtt_client_handle_t>(client_),
            topic.c_str(), data.c_str(), 0, qos, retain ? 1 : 0) < 0)
        return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

bool MqttConn::is_connected() const noexcept { return connected_; }

} // namespace fpc
