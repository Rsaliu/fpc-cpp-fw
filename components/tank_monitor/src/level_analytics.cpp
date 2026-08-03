#include "tank_monitor.hpp"
#include "esp_log.h"

namespace fpc {

static const char* kAnalyticsTag = "level_analytics";



TankStateMachineState level_analytics_from_top(
    Span<const uint16_t> samples,
    int32_t full_level_mm,
    int32_t low_level_mm,
    int32_t container_height_mm) noexcept
{
    uint32_t sum = 0;

    if (samples.empty()) {
        ESP_LOGW(kAnalyticsTag, "Empty samples — Invalid State");
        return TankStateMachineState::InvalidState;
    }
    
    for (uint16_t v : samples) {
        sum += v;
    }
    const int32_t avg = static_cast<int32_t>(sum / samples.size());
    
    if (avg > container_height_mm){
        ESP_LOGE(kAnalyticsTag, "Error: Average is greater than container height");
        return TankStateMachineState::InvalidState;
    }

    uint32_t fluid_level = container_height_mm - avg;

    if(fluid_level >= full_level_mm){
        ESP_LOGW(kAnalyticsTag, "The tank is already full, can't take more liquid");
        return TankStateMachineState::Full;
    }
    if(fluid_level <= low_level_mm){
        ESP_LOGW(kAnalyticsTag, "The tank is low");
        return TankStateMachineState::Low;
    }
    ESP_LOGW(kAnalyticsTag, "The liquid is yet to full; has %lu mm to full", avg);
    return TankStateMachineState::Normal;
}


TankStateMachineState level_analytics_basic_decision(
    Span<const uint16_t> samples,
    int32_t full_level_mm,
    int32_t low_level_mm,
    int32_t container_height_mm) noexcept
{
    if (samples.empty()) {
        ESP_LOGW(kAnalyticsTag, "Empty samples — Invalid State");
        return TankStateMachineState::InvalidState;
    }

    uint32_t sum = 0;
    for (uint16_t v : samples) {
        sum += v;
    }
    const int32_t avg = static_cast<int32_t>(sum / samples.size());

    if (avg >= full_level_mm) {
        ESP_LOGW(kAnalyticsTag, "Full: avg=%d >= full_level=%d", (int)avg, (int)full_level_mm);
        return TankStateMachineState::Full;
    }
    if (avg <= low_level_mm) {
        ESP_LOGW(kAnalyticsTag, "Low: avg=%d <= low_level=%d", (int)avg, (int)low_level_mm);
        return TankStateMachineState::Low;
    }
    ESP_LOGI(kAnalyticsTag, "Normal: avg=%d", (int)avg);
    return TankStateMachineState::Normal;
}

}  // namespace fpc
