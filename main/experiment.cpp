/**
 * @file experiment.cpp
 * @brief Implementation of the profiling experiments described in
 *        docs/experiments/current_sensor_pump_monitor_profiling.md
 *
 * Uses only public component APIs (current_sensor, pump_monitor,
 * relay_driver, pump_control_unit, pump_monitor_task) — no new hardware
 * drivers are introduced.
 */

#include "experiment.hpp"

#include "current_sensor.hpp"
#include "pump_monitor.hpp"
#include "pump.hpp"
#include "relay_driver.hpp"
#include "pump_control_unit.hpp"
#include "pump_monitor_task.hpp"
#include "common.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace fpc::experiment {

namespace {

constexpr char TAG[] = "EXP";

// ─── Kconfig-derived constants (with safe fallbacks if not configured) ────────

#ifndef CONFIG_FPC_EXPERIMENT_ID
#define CONFIG_FPC_EXPERIMENT_ID 1
#endif
#ifndef CONFIG_FPC_EXPERIMENT_SAMPLE_COUNT
#define CONFIG_FPC_EXPERIMENT_SAMPLE_COUNT 1000
#endif
#ifndef CONFIG_FPC_EXPERIMENT_STEP_DELAY_MS
#define CONFIG_FPC_EXPERIMENT_STEP_DELAY_MS 3000
#endif
#ifndef CONFIG_FPC_EXPERIMENT_CHECK_INTERVAL_MS
#define CONFIG_FPC_EXPERIMENT_CHECK_INTERVAL_MS 100
#endif
#ifndef CONFIG_FPC_EXPERIMENT_PUMP_COUNT
#define CONFIG_FPC_EXPERIMENT_PUMP_COUNT 10
#endif
#ifndef CONFIG_FPC_EXPERIMENT_DURATION_S
#define CONFIG_FPC_EXPERIMENT_DURATION_S 30
#endif

constexpr int      kExperimentId       = CONFIG_FPC_EXPERIMENT_ID;
constexpr int      kSampleCount        = CONFIG_FPC_EXPERIMENT_SAMPLE_COUNT;
constexpr int      kStepDelayMs        = CONFIG_FPC_EXPERIMENT_STEP_DELAY_MS;
constexpr uint32_t kCheckIntervalMs    = CONFIG_FPC_EXPERIMENT_CHECK_INTERVAL_MS;
constexpr int      kPumpCount          = CONFIG_FPC_EXPERIMENT_PUMP_COUNT;
constexpr int      kDurationS          = CONFIG_FPC_EXPERIMENT_DURATION_S;

constexpr float kDefaultRatedCurrent      = 10.0f; ///< Amps, matches sample config.json
constexpr float kDefaultMinWorkingCurrent = 0.5f;  ///< Amps

// ─── Synthetic current sources (used when hardware backend == SYNTHETIC,
//     and always for experiments 4-7 which require repeatable injection) ──────

/// Returns a fixed value every call — used for latency/throughput baselines.
ReadCallback make_constant_synthetic_read_cb(float amps)
{
    return [amps]() -> Result<float> {
        return Result<float>::ok(amps);
    };
}

/// Returns `below` for the first `flip_after_calls` invocations, then `above`.
/// `flip_time_us_out`, if non-null, is stamped with esp_timer_get_time() at
/// the exact call that first returns `above`.
ReadCallback make_step_synthetic_read_cb(float below, float above,
                                          int flip_after_calls,
                                          std::shared_ptr<std::atomic<int64_t>> flip_time_us_out)
{
    auto call_count = std::make_shared<std::atomic<int>>(0);
    return [=]() -> Result<float> {
        int n = call_count->fetch_add(1);
        if (n < flip_after_calls) {
            return Result<float>::ok(below);
        }
        if (n == flip_after_calls && flip_time_us_out) {
            flip_time_us_out->store(esp_timer_get_time());
        }
        return Result<float>::ok(above);
    };
}

/// Builds a real hardware read callback per Kconfig-selected backend.
/// Falls back to synthetic if hardware init fails (e.g., no sensor attached).
ReadCallback make_backend_read_cb()
{
#if defined(CONFIG_FPC_EXPERIMENT_BACKEND_INTERNAL_ADC)
    InternalAdcAcs712Config cfg{};
    auto r = make_internal_adc_acs712_read_callback(cfg);
    if (r.is_ok()) {
        ESP_LOGI(TAG, "Using INTERNAL_ADC backend");
        return r.value();
    }
    ESP_LOGW(TAG, "INTERNAL_ADC init failed (%s) - falling back to synthetic",
             to_string(r.error()).data());
#elif defined(CONFIG_FPC_EXPERIMENT_BACKEND_ADS1115)
    Ads1115Acs712Config cfg{};
    auto r = make_ads1115_acs712_read_callback(cfg);
    if (r.is_ok()) {
        ESP_LOGI(TAG, "Using ADS1115 backend");
        return r.value();
    }
    ESP_LOGW(TAG, "ADS1115 init failed (%s) - falling back to synthetic",
             to_string(r.error()).data());
#endif
    ESP_LOGI(TAG, "Using SYNTHETIC backend");
    return make_constant_synthetic_read_cb(1.23f);
}

// ─── Experiment 1: current-read latency ──────────────────────────────────────

void run_exp1_read_latency()
{
    ESP_LOGI(TAG, "EXP1,begin,N=%d", kSampleCount);

    CurrentSensorConfig cfg{};
    cfg.id      = 1;
    cfg.make    = "EXP1";
    cfg.read_cb = make_backend_read_cb();
    CurrentSensor sensor{std::move(cfg)};

    if (auto r = sensor.init(); r.is_err()) {
        ESP_LOGE(TAG, "EXP1,init_failed,%s", to_string(r.error()).data());
        return;
    }

    int64_t sum_us = 0, min_us = INT64_MAX, max_us = 0;
    for (int i = 0; i < kSampleCount; ++i) {
        const int64_t t0 = esp_timer_get_time();
        auto res = sensor.read();
        const int64_t t1 = esp_timer_get_time();
        if (res.is_err()) {
            ESP_LOGW(TAG, "EXP1,%d,read_error,%s", i, to_string(res.error()).data());
            continue;
        }
        const int64_t dt = t1 - t0;
        sum_us += dt;
        if (dt < min_us) min_us = dt;
        if (dt > max_us) max_us = dt;
        ESP_LOGI(TAG, "EXP1,%d,%lld", i, static_cast<long long>(dt));
    }
    const double mean_us = static_cast<double>(sum_us) / kSampleCount;
    ESP_LOGI(TAG, "EXP1,summary,min_us=%lld,mean_us=%.2f,max_us=%lld",
             static_cast<long long>(min_us), mean_us, static_cast<long long>(max_us));

    (void)sensor.deinit();
}

// ─── Experiment 2: current-read accuracy sweep ───────────────────────────────

void run_exp2_accuracy_sweep()
{
    ESP_LOGI(TAG, "EXP2,begin,step_delay_ms=%d", kStepDelayMs);

    CurrentSensorConfig cfg{};
    cfg.id      = 2;
    cfg.make    = "EXP2";
    cfg.read_cb = make_backend_read_cb();
    CurrentSensor sensor{std::move(cfg)};

    if (auto r = sensor.init(); r.is_err()) {
        ESP_LOGE(TAG, "EXP2,init_failed,%s", to_string(r.error()).data());
        return;
    }

    // Operator-paced sweep: 0A up to rated current in 10% steps.
    constexpr int kSteps = 11;
    constexpr int kSamplesPerStep = 50;
    for (int step = 0; step < kSteps; ++step) {
        const float nominal = kDefaultRatedCurrent * (static_cast<float>(step) / (kSteps - 1));
        ESP_LOGI(TAG, "EXP2,set_reference_now,%.3f", nominal);
        vTaskDelay(pdMS_TO_TICKS(kStepDelayMs));

        double sum = 0.0, sum_sq = 0.0;
        int    ok  = 0;
        for (int i = 0; i < kSamplesPerStep; ++i) {
            auto res = sensor.read();
            if (res.is_err()) continue;
            const double v = res.value();
            sum += v;
            sum_sq += v * v;
            ++ok;
        }
        if (ok == 0) {
            ESP_LOGW(TAG, "EXP2,%.3f,no_valid_samples", nominal);
            continue;
        }
        const double mean = sum / ok;
        const double var  = (sum_sq / ok) - (mean * mean);
        const double stddev = var > 0.0 ? std::sqrt(var) : 0.0;
        ESP_LOGI(TAG, "EXP2,%.3f,%.4f,%.4f", nominal, mean, stddev);
    }

    (void)sensor.deinit();
}

// ─── Experiment 3: consecutive-read throughput via check_current() ─────────

void run_exp3_throughput()
{
    ESP_LOGI(TAG, "EXP3,begin");

    const int sample_counts[] = {1, 5, 10, 20, 50};
    for (int ns : sample_counts) {
        PumpConfig pump_cfg{};
        pump_cfg.id                  = 3;
        pump_cfg.current_rating      = kDefaultRatedCurrent;
        pump_cfg.min_working_current = kDefaultMinWorkingCurrent;

        PumpMonitorConfig cfg{};
        cfg.id                = 3;
        cfg.pump_config        = pump_cfg;
        cfg.read_cb            = make_backend_read_cb();
        cfg.number_of_samples  = ns;
        cfg.analytics_cb       = current_analytics_basic_decision;

        PumpMonitor monitor{cfg};
        if (auto r = monitor.init(); r.is_err()) {
            ESP_LOGE(TAG, "EXP3,%d,init_failed,%s", ns, to_string(r.error()).data());
            continue;
        }

        constexpr int kIterations = 100;
        int64_t sum_us = 0;
        for (int i = 0; i < kIterations; ++i) {
            const int64_t t0 = esp_timer_get_time();
            (void)monitor.check_current();
            const int64_t t1 = esp_timer_get_time();
            sum_us += (t1 - t0);
        }
        const double mean_us = static_cast<double>(sum_us) / kIterations;
        const double hz = mean_us > 0.0 ? 1'000'000.0 / mean_us : 0.0;
        ESP_LOGI(TAG, "EXP3,%d,%.2f,%.2f", ns, mean_us, hz);

        (void)monitor.deinit();
    }
}

// ─── Experiment 4: overcurrent detection tolerance ───────────────────────────

void run_exp4_overcurrent_tolerance()
{
    ESP_LOGI(TAG, "EXP4,begin,rated=%.3f", kDefaultRatedCurrent);

    PumpConfig pump_cfg{};
    pump_cfg.id                  = 4;
    pump_cfg.current_rating      = kDefaultRatedCurrent;
    pump_cfg.min_working_current = kDefaultMinWorkingCurrent;

    auto injected = std::make_shared<std::atomic<float>>(kDefaultRatedCurrent);
    ReadCallback synthetic_cb = [injected]() -> Result<float> {
        return Result<float>::ok(injected->load());
    };

    PumpMonitorConfig cfg{};
    cfg.id               = 4;
    cfg.pump_config       = pump_cfg;
    cfg.read_cb           = synthetic_cb;
    cfg.number_of_samples = 5;
    cfg.analytics_cb      = current_analytics_basic_decision;

    PumpMonitor monitor{cfg};
    if (auto r = monitor.init(); r.is_err()) {
        ESP_LOGE(TAG, "EXP4,init_failed,%s", to_string(r.error()).data());
        return;
    }

    auto last_event = std::make_shared<std::atomic<int>>(-1);
    auto sub = monitor.subscribe([last_event](EventType e, int32_t) {
        last_event->store(static_cast<int>(e));
    });
    if (sub.is_err()) {
        ESP_LOGE(TAG, "EXP4,subscribe_failed,%s", to_string(sub.error()).data());
        return;
    }

    // Sweep 0% -> 20% above rating in 1% steps.
    for (int pct = 0; pct <= 20; ++pct) {
        const float amps = kDefaultRatedCurrent * (1.0f + (static_cast<float>(pct) / 100.0f));
        injected->store(amps);
        (void)monitor.check_current();
        const int ev = last_event->load();
        const char* ev_str = (ev >= 0) ? to_string(static_cast<EventType>(ev)).data() : "none";
        ESP_LOGI(TAG, "EXP4,%d,%.4f,%s", pct, amps, ev_str);
    }

    (void)monitor.unsubscribe(sub.value());
    (void)monitor.deinit();
}

// ─── Experiment 5: state-change reaction latency ─────────────────────────────

void run_exp5_reaction_latency()
{
    ESP_LOGI(TAG, "EXP5,begin,trials=30");

    constexpr int kTrials = 30;
    constexpr int kFlipAfterCalls = 5;

    for (int trial = 0; trial < kTrials; ++trial) {
        PumpConfig pump_cfg{};
        pump_cfg.id                  = 5;
        pump_cfg.current_rating      = kDefaultRatedCurrent;
        pump_cfg.min_working_current = kDefaultMinWorkingCurrent;

        auto flip_time_us = std::make_shared<std::atomic<int64_t>>(0);
        ReadCallback step_cb = make_step_synthetic_read_cb(
            kDefaultRatedCurrent * 0.5f,   // below: normal running current
            kDefaultRatedCurrent * 1.5f,   // above: overcurrent
            kFlipAfterCalls, flip_time_us);

        PumpMonitorConfig cfg{};
        cfg.id               = 5;
        cfg.pump_config       = pump_cfg;
        cfg.read_cb           = step_cb;
        cfg.number_of_samples = 1;
        cfg.analytics_cb      = current_analytics_basic_decision;

        PumpMonitor monitor{cfg};
        if (auto r = monitor.init(); r.is_err()) {
            ESP_LOGE(TAG, "EXP5,%d,init_failed,%s", trial, to_string(r.error()).data());
            continue;
        }

        auto reaction_time_us = std::make_shared<std::atomic<int64_t>>(0);
        auto sub = monitor.subscribe([reaction_time_us](EventType e, int32_t) {
            if (e == EventType::PumpOvercurrent) {
                reaction_time_us->store(esp_timer_get_time());
            }
        });
        if (sub.is_err()) {
            ESP_LOGE(TAG, "EXP5,%d,subscribe_failed,%s", trial, to_string(sub.error()).data());
            continue;
        }

        // Drive enough check_current() calls to pass the flip point.
        for (int i = 0; i < kFlipAfterCalls + 2; ++i) {
            (void)monitor.check_current();
        }

        const int64_t t0 = flip_time_us->load();
        const int64_t t1 = reaction_time_us->load();
        if (t0 > 0 && t1 > 0) {
            ESP_LOGI(TAG, "EXP5,%d,%lld", trial, static_cast<long long>(t1 - t0));
        } else {
            ESP_LOGW(TAG, "EXP5,%d,no_event_observed", trial);
        }

        (void)monitor.unsubscribe(sub.value());
        (void)monitor.deinit();
    }
}

// ─── Experiment 6: relay trip latency ────────────────────────────────────────

void run_exp6_relay_trip_latency()
{
    ESP_LOGI(TAG, "EXP6,begin,trials=30");

    constexpr int kTrials = 30;
    constexpr int kFlipAfterCalls = 5;
    constexpr gpio_num_t kRelayPin = GPIO_NUM_4; // matches sample config.json relay id=1

    EspGpioDriver gpio_driver;

    for (int trial = 0; trial < kTrials; ++trial) {
        RelayConfig relay_cfg{6, kRelayPin};
        Relay relay{relay_cfg, gpio_driver};
        if (auto r = relay.init(); r.is_err()) {
            ESP_LOGE(TAG, "EXP6,%d,relay_init_failed,%s", trial, to_string(r.error()).data());
            continue;
        }

        PumpConfig pump_cfg{};
        pump_cfg.id                  = 6;
        pump_cfg.current_rating      = kDefaultRatedCurrent;
        pump_cfg.min_working_current = kDefaultMinWorkingCurrent;

        auto flip_time_us = std::make_shared<std::atomic<int64_t>>(0);
        ReadCallback step_cb = make_step_synthetic_read_cb(
            kDefaultRatedCurrent * 0.5f,
            kDefaultRatedCurrent * 1.5f,
            kFlipAfterCalls, flip_time_us);

        PumpMonitorConfig cfg{};
        cfg.id               = 6;
        cfg.pump_config       = pump_cfg;
        cfg.read_cb           = step_cb;
        cfg.number_of_samples = 1;
        cfg.analytics_cb      = current_analytics_basic_decision;

        PumpMonitor monitor{cfg};
        if (auto r = monitor.init(); r.is_err()) {
            ESP_LOGE(TAG, "EXP6,%d,monitor_init_failed,%s", trial, to_string(r.error()).data());
            (void)relay.deinit();
            continue;
        }

        auto detect_time_us     = std::make_shared<std::atomic<int64_t>>(0);
        auto relay_call_time_us = std::make_shared<std::atomic<int64_t>>(0);
        auto* relay_ptr = &relay;

        auto sub = monitor.subscribe([detect_time_us, relay_call_time_us, relay_ptr]
                                      (EventType e, int32_t) {
            if (e == EventType::PumpOvercurrent) {
                const int64_t t1 = esp_timer_get_time();
                detect_time_us->store(t1);
                (void)relay_ptr->trip();
                const int64_t t2 = esp_timer_get_time();
                relay_call_time_us->store(t2 - t1);
            }
        });
        if (sub.is_err()) {
            ESP_LOGE(TAG, "EXP6,%d,subscribe_failed,%s", trial, to_string(sub.error()).data());
            (void)relay.deinit();
            continue;
        }

        for (int i = 0; i < kFlipAfterCalls + 2; ++i) {
            (void)monitor.check_current();
        }

        const int64_t t0 = flip_time_us->load();
        const int64_t t1 = detect_time_us->load();
        const int64_t relay_dt = relay_call_time_us->load();
        if (t0 > 0 && t1 > 0) {
            ESP_LOGI(TAG, "EXP6,%d,%lld,%lld", trial,
                     static_cast<long long>(t1 - t0),
                     static_cast<long long>(relay_dt));
        } else {
            ESP_LOGW(TAG, "EXP6,%d,no_event_observed", trial);
        }

        (void)monitor.unsubscribe(sub.value());
        (void)monitor.deinit();
        (void)relay.reset();
        (void)relay.deinit();
    }
}

// ─── Experiment 7: multi-pump scaling behavior ───────────────────────────────

void run_exp7_max_pump_scaling()
{
    ESP_LOGI(TAG, "EXP7,begin,pump_count=%d,check_interval_ms=%lu,duration_s=%d",
             kPumpCount, static_cast<unsigned long>(kCheckIntervalMs), kDurationS);

    std::vector<std::unique_ptr<PumpMonitor>>     monitors;
    std::vector<std::unique_ptr<PumpMonitorTask>> tasks;
    monitors.reserve(kPumpCount);
    tasks.reserve(kPumpCount);

    PumpControlUnit pcu;
    if (auto r = pcu.init(); r.is_err()) {
        ESP_LOGE(TAG, "EXP7,pcu_init_failed,%s", to_string(r.error()).data());
        return;
    }

    for (int p = 0; p < kPumpCount; ++p) {
        PumpConfig pump_cfg{};
        pump_cfg.id                  = p + 1;
        pump_cfg.current_rating      = kDefaultRatedCurrent;
        pump_cfg.min_working_current = kDefaultMinWorkingCurrent;

        // Each pump's synthetic read_cb logs its own call-to-call jitter.
        auto last_call_us = std::make_shared<std::atomic<int64_t>>(0);
        auto call_index    = std::make_shared<std::atomic<int>>(0);
        const int pump_id = p + 1;
        ReadCallback jitter_cb = [last_call_us, call_index, pump_id]() -> Result<float> {
            const int64_t now = esp_timer_get_time();
            const int64_t prev = last_call_us->exchange(now);
            const int idx = call_index->fetch_add(1);
            if (prev != 0) {
                ESP_LOGI(TAG, "EXP7,%d,%d,%lld", pump_id, idx,
                         static_cast<long long>(now - prev));
            }
            return Result<float>::ok(kDefaultRatedCurrent * 0.5f);
        };

        PumpMonitorConfig cfg{};
        cfg.id                = pump_id;
        cfg.pump_config        = pump_cfg;
        cfg.read_cb            = jitter_cb;
        cfg.number_of_samples  = 1;
        cfg.analytics_cb       = current_analytics_basic_decision;

        auto monitor = std::make_unique<PumpMonitor>(cfg);
        if (auto r = monitor->init(); r.is_err()) {
            ESP_LOGE(TAG, "EXP7,%d,init_failed,%s", pump_id, to_string(r.error()).data());
            continue;
        }
        if (auto r = pcu.add_pump_monitor(*monitor); r.is_err()) {
            ESP_LOGE(TAG, "EXP7,%d,add_failed,%s", pump_id, to_string(r.error()).data());
            continue;
        }

        PumpMonitorTaskConfig task_cfg{};
        task_cfg.id                = pump_id;
        task_cfg.monitor           = monitor.get();
        task_cfg.check_interval_ms = kCheckIntervalMs;

        auto task = std::make_unique<PumpMonitorTask>(task_cfg);
        if (auto r = task->start(); r.is_err()) {
            ESP_LOGE(TAG, "EXP7,%d,task_start_failed,%s", pump_id, to_string(r.error()).data());
            continue;
        }

        monitors.push_back(std::move(monitor));
        tasks.push_back(std::move(task));
    }

    ESP_LOGI(TAG, "EXP7,running,%d,pumps_active", static_cast<int>(tasks.size()));
    vTaskDelay(pdMS_TO_TICKS(static_cast<uint32_t>(kDurationS) * 1000U));

    for (auto& task : tasks) {
        (void)task->stop();
    }
    tasks.clear();
    for (auto& monitor : monitors) {
        (void)monitor->deinit();
    }
    monitors.clear();

    ESP_LOGI(TAG, "EXP7,end");
}

} // namespace

void run()
{
    ESP_LOGI(TAG, "Experiment firmware starting, EXPERIMENT_ID=%d", kExperimentId);

    switch (kExperimentId) {
        case 1: run_exp1_read_latency();          break;
        case 2: run_exp2_accuracy_sweep();        break;
        case 3: run_exp3_throughput();            break;
        case 4: run_exp4_overcurrent_tolerance();  break;
        case 5: run_exp5_reaction_latency();      break;
        case 6: run_exp6_relay_trip_latency();    break;
        case 7: run_exp7_max_pump_scaling();      break;
        default:
            ESP_LOGE(TAG, "Unknown FPC_EXPERIMENT_ID=%d", kExperimentId);
            break;
    }

    ESP_LOGI(TAG, "Experiment complete — idling");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

} // namespace fpc::experiment
