/**
 * @file tank_monitor.hpp
 * @brief Tank-monitor abstraction — ITankMonitor interface +
 *        TankMonitor concrete implementation with injectable callbacks.
 *
 * Architecture:
 *
 *   ITankMonitor            ← pure interface; other components depend on this.
 *       └─ TankMonitor      ← concrete; wired at construction time via
 *                              injected LevelReadCallback and
 *                              LevelAnalyticsCallback (std::function).
 *
 * C++17 improvements over the reference C implementation:
 *  - No `void*` context    — lambda captures replace context pointers.
 *  - No raw `tank_t*`      — TankConfig held by value inside the config struct.
 *  - No raw `level_sensor_t*` + separate callback — single LevelReadCallback.
 *  - `Result<T>`            — replaces `error_type_t` + output parameters.
 *  - Analytics injected as `LevelAnalyticsCallback` — fully testable.
 *  - `TankStateMachineState` / `TankMonitorState` as `enum class`.
 */

#pragma once

#include <cstdint>
#include <functional>
#include "common.hpp"
#include "event.hpp"
#include "tank.hpp"

namespace fpc {

// ─── State machine state ──────────────────────────────────────────────────────

enum class TankStateMachineState : uint8_t {
    Normal = 0, ///< Level is within normal operating range.
    Full   = 1, ///< Level has reached or exceeded full_level_mm.
    Low    = 2, ///< Level has dropped to or below low_level_mm.
};

// ─── Analytics ────────────────────────────────────────────────────────────────

/// Injected analytics callback.
/// Receives (samples, full_level_mm, low_level_mm) and returns the new state.
using LevelAnalyticsCallback = std::function<
    TankStateMachineState(Span<const uint16_t>, int32_t, int32_t)>;

/// Built-in average-based decision function.
/// Usable directly as a LevelAnalyticsCallback or in unit tests.
[[nodiscard]] TankStateMachineState level_analytics_basic_decision(
    Span<const uint16_t> samples,
    int32_t full_level_mm,
    int32_t low_level_mm) noexcept;

// ─── Callbacks ────────────────────────────────────────────────────────────────

/// Zero-argument callable that returns a raw level reading in mm.
using LevelReadCallback = std::function<Result<uint16_t>()>;

/// Subscriber event callback: receives event type and subscriber slot id.
using TankMonitorEventCallback = std::function<void(EventType, int32_t)>;

// ─── Subscriber ───────────────────────────────────────────────────────────────

struct TankMonitorSubscriber {
    int32_t                  id{-1};
    TankMonitorEventCallback callback;
    bool                     in_use{false};
};

// ─── Config ───────────────────────────────────────────────────────────────────

struct TankMonitorConfig {
    int32_t                id{0};
    TankConfig             tank_config{};         ///< Provides full/low level thresholds.
    LevelReadCallback      level_read_cb;         ///< std::function<Result<uint16_t>()>
    int32_t                number_of_samples{1};  ///< 1 .. TankMonitor::kMaxSamples
    LevelAnalyticsCallback analytics_cb;
};

// ─── Monitor state ────────────────────────────────────────────────────────────

enum class TankMonitorState : uint8_t {
    NotInitialized = 0,
    Initialized    = 1,
};

// ─── ITankMonitor interface ───────────────────────────────────────────────────

class ITankMonitor {
public:
    virtual ~ITankMonitor() = default;

    [[nodiscard]] virtual int32_t         id()          const noexcept = 0;
    [[nodiscard]] virtual Result<void>    init()        = 0;
    [[nodiscard]] virtual Result<void>    deinit()      = 0;
    [[nodiscard]] virtual Result<void>    check_level() = 0;
    [[nodiscard]] virtual TankMonitorState state() const noexcept = 0;

    [[nodiscard]] virtual Result<int32_t> subscribe(TankMonitorEventCallback callback) = 0;
    [[nodiscard]] virtual Result<void>    unsubscribe(int32_t event_id)                = 0;
};

// ─── TankMonitor ──────────────────────────────────────────────────────────────

class TankMonitor final : public ITankMonitor {
public:
    static constexpr int32_t kMaxSubscribers = 10;
    static constexpr int32_t kMaxSamples     = 50;

    explicit TankMonitor(TankMonitorConfig config);
    ~TankMonitor() override = default;

    TankMonitor(const TankMonitor&)            = delete;
    TankMonitor& operator=(const TankMonitor&) = delete;

    [[nodiscard]] int32_t     id()     const noexcept override;
    [[nodiscard]] Result<void> init()   override;
    [[nodiscard]] Result<void> deinit() override;

    /// Samples level number_of_samples times, runs analytics, and notifies
    /// subscribers if the state machine state has changed.
    [[nodiscard]] Result<void> check_level() override;

    [[nodiscard]] TankMonitorState          state()    const noexcept override;
    [[nodiscard]] const TankMonitorConfig&  config()   const noexcept;
    [[nodiscard]] TankStateMachineState     sm_state() const noexcept;

    /// Render a human-readable info line into `buf`.
    /// Returns BufferOverflow if the formatted string does not fit.
    [[nodiscard]] Result<void> format_info_into(MutableByteView buf) const noexcept;

    [[nodiscard]] Result<int32_t> subscribe(TankMonitorEventCallback callback) override;
    [[nodiscard]] Result<void>    unsubscribe(int32_t event_id)                override;

private:
    TankMonitorConfig     config_;
    uint16_t              samples_[kMaxSamples]{};
    TankMonitorState      state_{TankMonitorState::NotInitialized};
    TankStateMachineState sm_state_{TankStateMachineState::Normal};
    TankMonitorSubscriber subscribers_[kMaxSubscribers]{};
    int32_t               subscriber_count_{0};

    static EventType sm_state_to_event(TankStateMachineState s) noexcept;
    void             notify_subscribers(EventType event) noexcept;
};

}  // namespace fpc
