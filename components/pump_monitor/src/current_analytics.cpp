#include "pump_monitor.hpp"
#include <cmath>
#include "esp_log.h"

namespace fpc {

static const char* kAnalyticsTag = "current_analytics";

// using bubble sorts to sort through the given samples and then picking the 3 largest samples
// then the average of the samples are then used to compare current-rating.

void sorter(float arr[], int n){
    for (int i = 0; i < n-1; i++){
        for (int j = 0; j+1<n; j++){
            if(arr[j] < arr[j+1]){
                float temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }  

        }
    }
    
}

PumpStateMachineState current_analytics_basic_decision(
    Span<const float> samples,
    float rated_current,
    float min_working_current) noexcept
{
    if (samples.empty()) {
        return PumpStateMachineState::Undercurrent;
    }

    std::vector<float> sorted;
    for (int i = 0; i < samples.size(); i++){
        sorted.push_back(samples[i]);
    }
    sorter(sorted.data(), sorted.size());
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
    const float percentage_threshold = max_rating * 0.8;

    if (rated_current > 0 && rated_current >= percentage_threshold) {
        ESP_LOGW(kAnalyticsTag, "Overcurrent: 80%% value=%.2f, rated=%.2f", percentage_threshold , rated_current);
        return PumpStateMachineState::Overcurrent;
    }

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
