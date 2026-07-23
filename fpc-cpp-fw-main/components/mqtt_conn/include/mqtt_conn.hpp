/**
 * @file mqtt_conn.hpp
 * @brief RAII MQTT client wrapper around esp-mqtt.
 *
 * Incoming messages are pushed onto a caller-supplied QueueHandle_t as
 * MqttMessage structs.  init() blocks until connected or failed.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <string>
#include <cstdint>
#include "common.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

namespace fpc {

struct MqttMessage {
    char topic[128];
    char data[512];
};

struct MqttConfig {
    std::string   uri{};
    int           port{8883};
    std::string   client_id{};
    const char*   ca_cert_pem{nullptr};
    const char*   client_cert_pem{nullptr};
    const char*   client_key_pem{nullptr};
    QueueHandle_t message_queue{nullptr};
};

class MqttConn final {
public:
    explicit MqttConn(MqttConfig config);
    ~MqttConn();

    MqttConn(const MqttConn&)            = delete;
    MqttConn& operator=(const MqttConn&) = delete;

    [[nodiscard]] Result<void> init();
    [[nodiscard]] Result<void> deinit();
    [[nodiscard]] Result<void> subscribe(const std::string& topic, int qos = 0);
    [[nodiscard]] Result<void> unsubscribe(const std::string& topic);
    [[nodiscard]] Result<void> publish(const std::string& topic,
                                        const std::string& data,
                                        int  qos    = 0,
                                        bool retain = false);
    [[nodiscard]] bool is_connected() const noexcept;

private:
    static void event_handler(void* handler_args,
                               esp_event_base_t base,
                               int32_t          event_id,
                               void*            event_data);

    MqttConfig         config_;
    void*              client_{nullptr};
    EventGroupHandle_t event_group_{nullptr};
    bool               connected_{false};

    static constexpr int kConnectedBit   = BIT0;
    static constexpr int kFailBit        = BIT1;
    static constexpr int kMaxQueueWaitMs = 1000;
};

} // namespace fpc
