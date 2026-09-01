#include "pump_monitor.hpp"
#include <algorithm>
#include <cmath>
#include "esp_log.h"

namespace fpc {

static const char* kAnalyticsTag = "current_analytics";

constexpr float percentage = 0.8f;
constexpr float max_non_working_current = 0.1f;

PumpStateMachineState current_analytics_capacity_decision(
    Span<const float> samples,
    float rated_current,
    float min_working_current) noexcept
{
    
    if (rated_current <= 0){
        ESP_LOGE(kAnalyticsTag, "Rated current must be greater than 0");
        return PumpStateMachineState::Invalid;
    }

    if (samples.empty()) {
        ESP_LOGE(kAnalyticsTag, "Samples cannot be empty");
        return PumpStateMachineState::Invalid;
    }

    std::vector<float> sorted;
    for (int i = 0; i < samples.size(); i++){
        sorted.push_back(samples[i]);
    }

    std::sort(sorted.begin(), sorted.end(), std::greater<float>());
    size_t count = sorted.size() < 3 ? sorted.size() : 3;
    
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++){
        float v = sorted[i];
        sum += std::fabs(v);
    }
    
    // using the average of the 3 largest samples to compare with the rated current and min working current
    const float avg = sum / static_cast<float>(count);

    //a variant that read the rated threshold at 80% of the rated current.
    const float max_rating = rated_current;
    const float percentage_threshold = max_rating * percentage;

    if (avg >= percentage_threshold) {
        ESP_LOGW(kAnalyticsTag, "Overcurrent: 80%% value=%.2f, rated=%.2f", percentage_threshold , rated_current);
        return PumpStateMachineState::Overcurrent;
    }
    else if (avg < min_working_current && avg > max_non_working_current) {
        ESP_LOGW(kAnalyticsTag, "Undercurrent: avg=%.2f, min=%.2f", avg, min_working_current);
        return PumpStateMachineState::Undercurrent;
    }
    else if (avg <= max_non_working_current) {
        ESP_LOGW(kAnalyticsTag, "off: avg=%.2f, min=%.2f", avg, min_working_current);
        return PumpStateMachineState::Off;
    }
    else{
        ESP_LOGI(kAnalyticsTag, "Normal: avg=%.2f", avg);
        return PumpStateMachineState::Normal;
    }
}

 

PumpStateMachineState current_analytics_basic_decision(
    Span<const float> samples,
    float rated_current,
    float min_working_current) noexcept
{
    if (samples.empty()) {
        ESP_LOGE(kAnalyticsTag, "Samples cannot be empty");
        return PumpStateMachineState::Invalid;
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
