#include "pump_monitor.hpp"
#include <cmath>
#include "esp_log.h"

namespace fpc {

static const char* kAnalyticsTag = "current_analytics";

PumpStateMachineState current_analytics_basic_decision(
    Span<const float> samples,
    float rated_current,
    float min_working_current) noexcept
{
    if (samples.empty()) {
        return PumpStateMachineState::Undercurrent;
    }

    float sum = 0.0f;
    for (float v : samples) {
        sum += std::fabs(v);
    }
    const float avg = sum / static_cast<float>(samples.size());

    if (avg >= rated_current) {
        ESP_LOGW(kAnalyticsTag, "Overcurrent: avg=%.2f, rated=%.2f", avg, rated_current);
        return PumpStateMachineState::Overcurrent;
    }
    if (avg >= min_working_current) {
        ESP_LOGI(kAnalyticsTag, "Normal: avg=%.2f", avg);
        return PumpStateMachineState::Normal;
    }
    ESP_LOGW(kAnalyticsTag, "Undercurrent: avg=%.2f, min=%.2f", avg, min_working_current);
    return PumpStateMachineState::Undercurrent;
}

}  // namespace fpc
