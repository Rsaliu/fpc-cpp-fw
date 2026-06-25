#include "comms_manager.hpp"
#include <cstring>
#include "esp_log.h"

static const char* TAG = "COMMS_MANAGER";
static constexpr int kQueueWaitMs = 100;

namespace fpc {

RouteManager::RouteManager(RouteManagerConfig config) : config_{std::move(config)}
{
    routes_.reserve(kMaxRoutes / 2);
}

Result<void> RouteManager::init()
{
    if (config_.message_queue == nullptr)
        return Result<void>::err(SystemError::NullParameter);
    initialized_ = true;
    return Result<void>::ok();
}

Result<void> RouteManager::add_route(const std::string& topic, RouteHandler handler)
{
    if (topic.empty())  return Result<void>::err(SystemError::InvalidParameter);
    if (!handler)       return Result<void>::err(SystemError::InvalidParameter);
    if (!initialized_)  return Result<void>::err(SystemError::InvalidState);
    if (routes_.size() >= kMaxRoutes) return Result<void>::err(SystemError::BufferOverflow);
    routes_[topic] = std::move(handler);
    return Result<void>::ok();
}

Result<void> RouteManager::handle_request()
{
    if (!initialized_)           return Result<void>::err(SystemError::InvalidState);
    if (!config_.message_queue)  return Result<void>::err(SystemError::NullParameter);

    MqttMessage msg{};
    if (xQueueReceive(config_.message_queue, &msg,
                      pdMS_TO_TICKS(kQueueWaitMs)) != pdPASS)
        return Result<void>::err(SystemError::Failed);

    auto it = routes_.find(std::string(msg.topic));
    if (it == routes_.end()) {
        ESP_LOGE(TAG, "No route for topic: '%s'", msg.topic);
        return Result<void>::err(SystemError::InvalidResponse);
    }
    it->second(std::string(msg.data));
    return Result<void>::ok();
}

bool        RouteManager::is_initialized() const noexcept { return initialized_; }
std::size_t RouteManager::route_count()    const noexcept { return routes_.size(); }

} // namespace fpc
