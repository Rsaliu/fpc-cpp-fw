#include "unity.h"
#include "tank_monitor.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "test_tank_monitor";

// ─── Test helpers ─────────────────────────────────────────────────────────────

/// Build a TankMonitorConfig whose callbacks capture caller-owned variables
/// by reference.  All lambdas remain valid for the lifetime of those variables.
static fpc::TankMonitorConfig make_config(
    uint16_t& mock_level,
    fpc::TankStateMachineState& analytics_state,
    int32_t number_of_samples = 5)
{
    fpc::TankConfig tank_cfg;
    tank_cfg.id             = 1;
    tank_cfg.capacity_litres = 1000.0f;
    tank_cfg.shape          = fpc::TankShape::Rectangle;
    tank_cfg.height_cm      = 100.0f;
    tank_cfg.full_level_mm  = 900;
    tank_cfg.low_level_mm   = 100;

    fpc::TankMonitorConfig cfg;
    cfg.id               = 1;
    cfg.tank_config      = tank_cfg;
    cfg.number_of_samples = number_of_samples;

    cfg.level_read_cb = [&mock_level]() -> fpc::Result<uint16_t> {
        return fpc::Result<uint16_t>::ok(mock_level);
    };

    cfg.analytics_cb = [&analytics_state](
        fpc::Span<const uint16_t> samples,
        int32_t full_mm,
        int32_t low_mm,
        int32_t container_height_mm) -> fpc::TankStateMachineState
    {
        if (samples.empty()) {
            return fpc::TankStateMachineState::InvalidState;
        }
        int32_t sum = 0;
        for (uint16_t v : samples) { sum += v; }
        const int32_t avg = sum / static_cast<int32_t>(samples.size());

        fpc::TankStateMachineState s;
        if (avg >= full_mm) {
            s = fpc::TankStateMachineState::Full;
        } else if (avg <= low_mm) {
            s = fpc::TankStateMachineState::Low;
        } else {
            s = fpc::TankStateMachineState::Normal;
        }
        analytics_state = s;
        ESP_LOGI(TAG, "analytics_cb: avg=%d → state=%d", (int)avg, (int)s);
        return s;
    };

    return cfg;
}

// ─── level_analytics_basic_decision ──────────────────────────────────────────

TEST_CASE("basic_decision: normal range", "[tank_monitor]")
{
    const uint16_t samples[] = {500, 600, 550}; // average = 550
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Normal, state);
}

TEST_CASE("basic_decision: at full level → Full", "[tank_monitor]")
{
    const uint16_t samples[] = {900, 910, 920}; // average = 910
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Full, state);
}

TEST_CASE("basic_decision: at low level → Low", "[tank_monitor]")
{
    const uint16_t samples[] = {100, 80, 60}; // average = 80
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Low, state);
}

TEST_CASE("basic_decision: empty span → InvalidState", "[tank_monitor]")
{
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::InvalidState, state);
}

// ----------- Level_analytics_from_top---------------------------------
TEST_CASE("from_top: normal range", "[tank_monitor]"){
    const uint16_t samples[] = {500, 600, 550}; // average = 550
    auto state = fpc::level_analytics_from_top(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Normal, state);
}

TEST_CASE("from_top: at full level", "[tank_monitor]"){
    const uint16_t samples[] = {100, 90, 80}; // average = 90
    auto state = fpc::level_analytics_from_top(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Full, state);
}

TEST_CASE("from_top: at low level", "[tank_monitor]"){
    const uint16_t samples[] = {900, 950, 920}; // average = 923.33
    auto state = fpc::level_analytics_from_top(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Low, state);
}

TEST_CASE("from_top: at invlid state", "[tank_monitor]"){
    const uint16_t samples[] = {1050, 1100, 1300}; // average = 1150
    auto state = fpc::level_analytics_from_top(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100, 1000);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::InvalidState, state);
}

// ─── TankMonitor lifecycle ────────────────────────────────────────────────────

TEST_CASE("TankMonitor: create starts NotInitialized", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    TEST_ASSERT_EQUAL(fpc::TankMonitorState::NotInitialized, tm.state());
}

TEST_CASE("TankMonitor: init succeeds", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    auto res = tm.init();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::TankMonitorState::Initialized, tm.state());
}

TEST_CASE("TankMonitor: double init returns InvalidState", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();
    auto res = tm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("TankMonitor: init fails with null level_read_cb", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    auto cfg = make_config(mock_level, analytics_state);
    cfg.level_read_cb = nullptr;

    fpc::TankMonitor tm{cfg};
    auto res = tm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::NullParameter, res.error());
}

TEST_CASE("TankMonitor: init fails with null analytics_cb", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    auto cfg = make_config(mock_level, analytics_state);
    cfg.analytics_cb = nullptr;

    fpc::TankMonitor tm{cfg};
    auto res = tm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::NullParameter, res.error());
}

TEST_CASE("TankMonitor: init fails with number_of_samples 0", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state, 0)};
    auto res = tm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("TankMonitor: init fails when number_of_samples exceeds kMaxSamples", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state,
                                    fpc::TankMonitor::kMaxSamples + 1)};
    auto res = tm.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("TankMonitor: deinit succeeds after init", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    // FIXED: Completed the lifecycle assertion loop
    auto res = tm.deinit();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::TankMonitorState::NotInitialized, tm.state());
}