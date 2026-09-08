#include "unity.h"
#include "pump_monitor.hpp"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "test_pump_monitor";

// ─── Test helpers ─────────────────────────────────────────────────────────────

/// Build a PumpMonitorConfig whose read_cb and analytics_cb capture the
/// caller-owned variables by reference.  All lambdas are valid for the
/// lifetime of those variables.
static fpc::PumpMonitorConfig make_config(
    float& mock_current,
    fpc::PumpStateMachineState& analytics_state)
{
    fpc::PumpConfig pump_cfg;
    pump_cfg.id                  = 1;
    pump_cfg.make                = "TestPump";
    pump_cfg.power_hp            = 2.5f;
    pump_cfg.current_rating      = 6.0f;
    pump_cfg.min_working_current = 0.5f;

    fpc::PumpMonitorConfig cfg;
    cfg.id              = 1;
    cfg.pump_config     = pump_cfg;
    cfg.number_of_samples = 5;

    cfg.read_cb = [&mock_current]() -> fpc::Result<float> {
        return fpc::Result<float>::ok(mock_current);
    };

    cfg.analytics_cb = [&analytics_state](
        fpc::Span<const float> samples, float rated, float min_w)
        -> fpc::PumpStateMachineState
    {
        const float sample = samples.empty() ? 0.0f : samples[0];
        fpc::PumpStateMachineState s;
        if (sample < min_w) {
            s = fpc::PumpStateMachineState::Undercurrent;
        } else if (sample > rated) {
            s = fpc::PumpStateMachineState::Overcurrent;
        } else {
            s = fpc::PumpStateMachineState::Normal;
        }
        analytics_state = s;
        ESP_LOGI(TAG, "analytics_cb: sample=%.2f → state=%d", sample, (int)s);
        return s;
    };

    return cfg;
}

// ─── current_analytics_basic_decision ────────────────────────────────────────

TEST_CASE("basic_decision: normal range", "[pump_monitor]")
{
    const float samples[] = {3.0f, 2.5f, 3.5f};
    auto state = fpc::current_analytics_basic_decision(
        fpc::Span<const float>{samples, 3}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Normal, state);
}

TEST_CASE("basic_decision: undercurrent", "[pump_monitor]")
{
    const float samples[] = {0.1f};
    auto state = fpc::current_analytics_basic_decision(
        fpc::Span<const float>{samples, 1}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Undercurrent, state);
}

TEST_CASE("basic_decision: overcurrent", "[pump_monitor]")
{
    const float samples[] = {7.0f};
    auto state = fpc::current_analytics_basic_decision(
        fpc::Span<const float>{samples, 1}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Overcurrent, state);
}

TEST_CASE("basic_decision: empty span returns Invalid", "[pump_monitor]")
{
    auto state = fpc::current_analytics_basic_decision(
        fpc::Span<const float>{}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Invalid, state);
}

// ─── current_analytics_capacity_decision ────────────────────────────────────────

TEST_CASE("capacity_decision: 0 rated_current returns Invalid", "[pump_monitor]")
{
    const float samples[] = {3.0f, 2.5f, 3.5f};
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{samples, 3}, 0.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Invalid, state);
}

TEST_CASE("capacity_decision: empty span returns Invalid", "[pump_monitor]")
{
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Invalid, state);
}

TEST_CASE("capacity_decision: overcurrent", "[pump_monitor]")
{
    const float samples[] = {7.0f};
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{samples, 1}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Overcurrent, state);
}

TEST_CASE("capacity_decision: undercurrent", "[pump_monitor]")
{
    const float samples[] = {0.2f, 0.25f,0.3f, 0.35f, 0.4f};
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{samples, 5}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Undercurrent, state);
}

TEST_CASE("capacity_decision: off", "[pump_monitor]")
{
    const float samples[] = {0.05f, 0.08f, 0.03f, 0.02f, 0.01f};
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{samples, 5}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Off, state);
}

TEST_CASE("capacity_decision: average of 3 largest samples", "[pump_monitor]")
{
    const float samples[] = {2.0f, 5.0f, 1.0f, 8.0f, 10.0f};
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{samples, 5}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Overcurrent, state);
}

TEST_CASE("capacity_decision: average of 2 largest samples when only 2 samples", "[pump_monitor]")
{
    const float samples[] = {2.0f, 5.0f};
    auto state = fpc::current_analytics_capacity_decision(
        fpc::Span<const float>{samples, 2}, 6.0f, 0.5f);
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Normal, state);
}

// ─── PumpMonitor lifecycle ────────────────────────────────────────────────────

TEST_CASE("PumpMonitor: create starts NotInitialized", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    TEST_ASSERT_EQUAL(fpc::PumpMonitorState::NotInitialized, pm.state());
}

TEST_CASE("PumpMonitor: init succeeds", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    auto res = pm.init();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpMonitorState::Initialized, pm.state());
}

TEST_CASE("PumpMonitor: double init fails with InvalidState", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    TEST_ASSERT_TRUE(pm.init().is_ok());
    auto res = pm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpMonitor: deinit succeeds after init", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    TEST_ASSERT_TRUE(pm.init().is_ok());
    auto res = pm.deinit();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpMonitorState::NotInitialized, pm.state());
}

TEST_CASE("PumpMonitor: deinit before init fails with InvalidState", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    auto res = pm.deinit();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpMonitor: init rejects invalid number_of_samples", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;
    auto cfg = make_config(mock_current, analytics_state);
    cfg.number_of_samples = 0;

    fpc::PumpMonitor pm{std::move(cfg)};
    auto res = pm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("PumpMonitor: check_current before init fails", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    auto res = pm.check_current();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

// ─── State machine transitions ────────────────────────────────────────────────

TEST_CASE("PumpMonitor: check_current state transitions trigger callbacks", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    TEST_ASSERT_TRUE(pm.init().is_ok());

    int32_t        event_count = 0;
    fpc::EventType last_event  = fpc::EventType::Unknown;
    int32_t        last_id     = -1;

    auto sub_res = pm.subscribe([&](fpc::EventType e, int32_t id) {
        ++event_count;
        last_event = e;
        last_id    = id;
    });
    TEST_ASSERT_TRUE(sub_res.is_ok());
    const int32_t event_id = sub_res.value();
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, event_id);

    // Normal → Normal: no notification (initial sm_state_ is Normal)
    mock_current = 3.0f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Normal, analytics_state);
    TEST_ASSERT_EQUAL_INT(0, event_count);

    // Normal → Undercurrent
    mock_current = 0.1f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Undercurrent, analytics_state);
    TEST_ASSERT_EQUAL_INT(1, event_count);
    TEST_ASSERT_EQUAL(fpc::EventType::PumpUndercurrent, last_event);
    TEST_ASSERT_EQUAL_INT(event_id, last_id);

    // Undercurrent → Normal
    mock_current = 3.0f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Normal, analytics_state);
    TEST_ASSERT_EQUAL_INT(2, event_count);
    TEST_ASSERT_EQUAL(fpc::EventType::PumpNormal, last_event);

    // Normal → Overcurrent
    mock_current = 7.0f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Overcurrent, analytics_state);
    TEST_ASSERT_EQUAL_INT(3, event_count);
    TEST_ASSERT_EQUAL(fpc::EventType::PumpOvercurrent, last_event);

    // Overcurrent → Overcurrent: no notification
    mock_current = 8.0f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL_INT(3, event_count);
}

// ─── Subscriber management ────────────────────────────────────────────────────

TEST_CASE("PumpMonitor: subscribe before init fails", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    auto res = pm.subscribe([](fpc::EventType, int32_t) {});
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpMonitor: unsubscribe stops callbacks", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    TEST_ASSERT_TRUE(pm.init().is_ok());

    int32_t event_count = 0;
    auto sub_res = pm.subscribe([&](fpc::EventType, int32_t) { ++event_count; });
    TEST_ASSERT_TRUE(sub_res.is_ok());
    const int32_t event_id = sub_res.value();

    // Trigger undercurrent → callback fires
    mock_current = 0.1f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL_INT(1, event_count);

    // Unsubscribe
    TEST_ASSERT_TRUE(pm.unsubscribe(event_id).is_ok());

    // Back to normal → state changes but callback NOT fired
    mock_current = 3.0f;
    TEST_ASSERT_TRUE(pm.check_current().is_ok());
    TEST_ASSERT_EQUAL(fpc::PumpStateMachineState::Normal, analytics_state);
    TEST_ASSERT_EQUAL_INT(1, event_count);  // still 1

    // Unsubscribe same id again → InvalidParameter
    auto res = pm.unsubscribe(event_id);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("PumpMonitor: unsubscribe invalid id fails", "[pump_monitor]")
{
    float mock_current = 3.0f;
    fpc::PumpStateMachineState analytics_state = fpc::PumpStateMachineState::Normal;

    fpc::PumpMonitor pm{make_config(mock_current, analytics_state)};
    TEST_ASSERT_TRUE(pm.init().is_ok());

    auto res = pm.unsubscribe(-1);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());

    res = pm.unsubscribe(fpc::PumpMonitor::kMaxSubscribers);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}
