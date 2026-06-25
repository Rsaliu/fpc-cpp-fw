/**
 * @file comms_manager.hpp
 * @brief MQTT topic → handler dispatch table (RouteManager).
 *
 * Reads MqttMessage structs from a shared QueueHandle_t and dispatches
 * to registered RouteHandler callbacks by topic string.
 * Uses std::unordered_map for O(1) average lookup.
 *
 * C++17 / fpc-cpp port of route_manager.h / route_manager.c.
 */

#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <cstddef>
#include "common.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_conn.hpp"

namespace fpc {

using RouteHandler = std::function<void(const std::string& request_data)>;

struct RouteManagerConfig {
    QueueHandle_t message_queue{nullptr};
};

class RouteManager final {
public:
    static constexpr std::size_t kMaxRoutes = 100U;

    explicit RouteManager(RouteManagerConfig config);

    RouteManager(const RouteManager&)            = delete;
    RouteManager& operator=(const RouteManager&) = delete;

    [[nodiscard]] Result<void> init();
    [[nodiscard]] Result<void> add_route(const std::string& topic, RouteHandler handler);
    [[nodiscard]] Result<void> handle_request();

    [[nodiscard]] bool        is_initialized() const noexcept;
    [[nodiscard]] std::size_t route_count()    const noexcept;

private:
    RouteManagerConfig                              config_;
    std::unordered_map<std::string, RouteHandler>   routes_;
    bool                                            initialized_{false};
};

} // namespace fpc
