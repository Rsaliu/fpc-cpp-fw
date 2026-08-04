#pragma once

#include <cstdint>
#include <functional>
#include "common.hpp"
#include "event.hpp"
#include "pump.hpp"
#include "current_sensor.hpp"   // for ReadCallback = std::function<Result<float>()>

namespace fpc {

// ─── State machine state ──────────────────────────────────────────────────────

enum class PumpStateMachineState : uint8_t {
    Normal       = 0,
    Undercurrent = 1,
    Overcurrent  = 2,
    Invalid     = 3,  ///< Used to indicate an error in the rated current reading or other unexpected condition.
};

// ─── Analytics ────────────────────────────────────────────────────────────────

/// Injected analytics: receives samples, rated_current, min_working_current
/// and returns the new state.
using AnalyticsCallback = std::function<
    PumpStateMachineState(Span<const float>, float, float)>;

/// Built-in average-based decision function.  Usable directly as an
/// AnalyticsCallback or in tests.
[[nodiscard]] PumpStateMachineState current_analytics_basic_decision(
    Span<const float> samples,
    float rated_current,
    float min_working_current) noexcept;

[[nodiscard]] PumpStateMachineState current_analytics_capacity_decision(
    Span<const float> samples,
    float rated_current,
    float min_working_current) noexcept;

// ─── Subscriber ───────────────────────────────────────────────────────────────

using PumpMonitorEventCallback = std::function<void(EventType, int32_t)>;

struct PumpMonitorSubscriber {
    int32_t                  id{-1};
    PumpMonitorEventCallback callback;
    bool                     in_use{false};
};

// ─── Config ───────────────────────────────────────────────────────────────────

struct PumpMonitorConfig {
    int32_t           id{0};
    PumpConfig        pump_config{};         ///< Provides current_rating and min_working_current
    ReadCallback      read_cb;               ///< std::function<Result<float>()>
    int32_t           number_of_samples{1};  ///< 1 .. PumpMonitor::kMaxSamples
    AnalyticsCallback analytics_cb;
};

// ─── Monitor state ────────────────────────────────────────────────────────────

enum class PumpMonitorState : uint8_t {
    NotInitialized = 0,
    Initialized    = 1,
};

// ─── IPumpMonitor interface ───────────────────────────────────────────────────

class IPumpMonitor {
public:
    virtual ~IPumpMonitor() = default;

    [[nodiscard]] virtual int32_t        id()            const noexcept = 0;
    [[nodiscard]] virtual Result<void>    init()          = 0;
    [[nodiscard]] virtual Result<void>    deinit()        = 0;
    [[nodiscard]] virtual Result<void>    check_current() = 0;
    [[nodiscard]] virtual PumpMonitorState state() const noexcept = 0;

    [[nodiscard]] virtual Result<int32_t> subscribe(PumpMonitorEventCallback callback) = 0;
    [[nodiscard]] virtual Result<void>    unsubscribe(int32_t event_id)                = 0;
};

// ─── PumpMonitor ──────────────────────────────────────────────────────────────

class PumpMonitor final : public IPumpMonitor {
public:
    static constexpr int32_t kMaxSubscribers = 10;
    static constexpr int32_t kMaxSamples     = 50;

    explicit PumpMonitor(PumpMonitorConfig config);
    ~PumpMonitor() override = default;

    PumpMonitor(const PumpMonitor&)            = delete;
    PumpMonitor& operator=(const PumpMonitor&) = delete;

    [[nodiscard]] int32_t         id()            const noexcept override;
    [[nodiscard]] Result<void>    init()          override;
    [[nodiscard]] Result<void>    deinit()        override;

    /// Samples current number_of_samples times, runs analytics, and notifies
    /// subscribers if the state machine state has changed.
    [[nodiscard]] Result<void>    check_current() override;

    [[nodiscard]] PumpMonitorState         state()  const noexcept override;
    [[nodiscard]] const PumpMonitorConfig& config() const noexcept;

    /// Registers a callback.  Returns the slot index (event_id) on success.
    [[nodiscard]] Result<int32_t> subscribe(PumpMonitorEventCallback callback) override;

    /// Removes the callback registered under event_id.
    [[nodiscard]] Result<void>    unsubscribe(int32_t event_id) override;

private:
    PumpMonitorConfig     config_;
    float                 samples_[kMaxSamples]{};
    PumpMonitorState      state_{PumpMonitorState::NotInitialized};
    PumpStateMachineState sm_state_{PumpStateMachineState::Normal};
    PumpMonitorSubscriber subscribers_[kMaxSubscribers]{};
    int32_t               subscriber_count_{0};

    static EventType sm_state_to_event(PumpStateMachineState s) noexcept;
    void             notify_subscribers(EventType event) noexcept;
};

}  // namespace fpc
