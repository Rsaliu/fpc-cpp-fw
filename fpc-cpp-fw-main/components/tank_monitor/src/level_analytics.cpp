#include "tank_monitor.hpp"
#include "esp_log.h"

namespace fpc {

static const char* kAnalyticsTag = "level_analytics";

TankStateMachineState level_analytics_basic_decision(
    Span<const uint16_t> samples,
    int32_t full_level_mm,
    int32_t low_level_mm) noexcept
{
    if (samples.empty()) {
        ESP_LOGW(kAnalyticsTag, "Empty samples — defaulting to Low");
        return TankStateMachineState::Low;
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
