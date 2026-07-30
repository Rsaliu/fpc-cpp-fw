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
        int32_t low_mm) -> fpc::TankStateMachineState
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
    const uint16_t samples[] = {500, 600, 550};
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Normal, state);
}

TEST_CASE("basic_decision: at full level → Full", "[tank_monitor]")
{
    const uint16_t samples[] = {900, 910, 920};
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Full, state);
}

TEST_CASE("basic_decision: at low level → Low", "[tank_monitor]")
{
    const uint16_t samples[] = {100, 80, 60};
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{samples, 3}, 900, 100);
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Low, state);
}

TEST_CASE("basic_decision: empty span → InvalidState", "[tank_monitor]")
{
    auto state = fpc::level_analytics_basic_decision(
        fpc::Span<const uint16_t>{}, 900, 100);
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
    auto res = tm.deinit();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::TankMonitorState::NotInitialized, tm.state());
}

TEST_CASE("TankMonitor: deinit fails when not initialized", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    auto res = tm.deinit();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

// ─── check_level ─────────────────────────────────────────────────────────────

TEST_CASE("TankMonitor: check_level fails when not initialized", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    auto res = tm.check_level();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("TankMonitor: check_level succeeds in normal range", "[tank_monitor]")
{
    uint16_t mock_level = 500;  // between 100 and 900
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();
    auto res = tm.check_level();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Normal, tm.sm_state());
}

TEST_CASE("TankMonitor: check_level transitions to Full", "[tank_monitor]")
{
    uint16_t mock_level = 950;  // above full_level_mm = 900
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();
    auto res = tm.check_level();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Full, tm.sm_state());
}

TEST_CASE("TankMonitor: check_level transitions to Low", "[tank_monitor]")
{
    uint16_t mock_level = 50;   // below low_level_mm = 100
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();
    auto res = tm.check_level();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(fpc::TankStateMachineState::Low, tm.sm_state());
}

TEST_CASE("TankMonitor: read_cb failure propagates through check_level", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    auto cfg = make_config(mock_level, analytics_state);
    cfg.level_read_cb = []() -> fpc::Result<uint16_t> {
        return fpc::Result<uint16_t>::err(fpc::SystemError::TimedOut);
    };

    fpc::TankMonitor tm{cfg};
    tm.init();
    auto res = tm.check_level();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::TimedOut, res.error());
}

// ─── Subscribe / Unsubscribe ──────────────────────────────────────────────────

TEST_CASE("TankMonitor: subscribe succeeds", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    auto res = tm.subscribe([](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_NOT_EQUAL(-1, res.value());
}

TEST_CASE("TankMonitor: subscribe fails when not initialized", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    auto res = tm.subscribe([](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("TankMonitor: subscribe with null callback returns NullParameter", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    auto res = tm.subscribe(nullptr);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::NullParameter, res.error());
}

TEST_CASE("TankMonitor: unsubscribe succeeds", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    auto sub_res = tm.subscribe([](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(sub_res.is_ok());

    auto unsub_res = tm.unsubscribe(sub_res.value());
    TEST_ASSERT_TRUE(unsub_res.is_ok());
}

TEST_CASE("TankMonitor: unsubscribe with invalid id returns InvalidParameter", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    auto res = tm.unsubscribe(99);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("TankMonitor: callback invoked on state transition to Full", "[tank_monitor]")
{
    uint16_t mock_level = 950;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    fpc::EventType last_event = fpc::EventType::Unknown;
    tm.subscribe([&last_event](fpc::EventType e, int32_t) {
        last_event = e;
    });

    tm.check_level();
    TEST_ASSERT_EQUAL(fpc::EventType::TankFull, last_event);
}

TEST_CASE("TankMonitor: callback invoked on state transition to Low", "[tank_monitor]")
{
    uint16_t mock_level = 50;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    fpc::EventType last_event = fpc::EventType::Unknown;
    tm.subscribe([&last_event](fpc::EventType e, int32_t) {
        last_event = e;
    });

    tm.check_level();
    TEST_ASSERT_EQUAL(fpc::EventType::TankLow, last_event);
}

TEST_CASE("TankMonitor: callback NOT invoked when state unchanged", "[tank_monitor]")
{
    uint16_t mock_level = 500;  // normal range → no transition
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    int call_count = 0;
    tm.subscribe([&call_count](fpc::EventType, int32_t) { ++call_count; });

    tm.check_level();  // Normal → Normal, no notification
    TEST_ASSERT_EQUAL(0, call_count);
}

TEST_CASE("TankMonitor: callback NOT invoked after unsubscribe", "[tank_monitor]")
{
    uint16_t mock_level = 950;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    int call_count = 0;
    auto sub_res = tm.subscribe([&call_count](fpc::EventType, int32_t) { ++call_count; });
    TEST_ASSERT_TRUE(sub_res.is_ok());

    tm.unsubscribe(sub_res.value());
    tm.check_level();  // would normally fire Full, but subscriber is gone
    TEST_ASSERT_EQUAL(0, call_count);
}

TEST_CASE("TankMonitor: overflow at kMaxSubscribers slots", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    for (int32_t i = 0; i < fpc::TankMonitor::kMaxSubscribers; ++i) {
        auto res = tm.subscribe([](fpc::EventType, int32_t){});
        TEST_ASSERT_TRUE(res.is_ok());
    }
    auto overflow = tm.subscribe([](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(overflow.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::BufferOverflow, overflow.error());
}

// ─── format_info_into ────────────────────────────────────────────────────────

TEST_CASE("TankMonitor: format_info_into writes expected content", "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    tm.init();

    uint8_t buf[128]{};
    auto res = tm.format_info_into(fpc::MutableByteView{buf, sizeof(buf)});
    TEST_ASSERT_TRUE(res.is_ok());

    const char* str = reinterpret_cast<const char*>(buf);
    ESP_LOGI(TAG, "format_info_into: %s", str);
    // Must contain the monitor id
    TEST_ASSERT_NOT_NULL(strstr(str, "ID=1"));
}

TEST_CASE("TankMonitor: format_info_into with empty buffer returns InvalidParameter",
          "[tank_monitor]")
{
    uint16_t mock_level = 500;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor tm{make_config(mock_level, analytics_state)};
    auto res = tm.format_info_into(fpc::MutableByteView{});
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

// ─── ITankMonitor polymorphism ────────────────────────────────────────────────

TEST_CASE("TankMonitor: usable through ITankMonitor pointer", "[tank_monitor]")
{
    uint16_t mock_level = 950;
    fpc::TankStateMachineState analytics_state = fpc::TankStateMachineState::Normal;

    fpc::TankMonitor concrete{make_config(mock_level, analytics_state)};
    fpc::ITankMonitor* pm = &concrete;

    TEST_ASSERT_TRUE(pm->init().is_ok());
    TEST_ASSERT_EQUAL(fpc::TankMonitorState::Initialized, pm->state());
    TEST_ASSERT_TRUE(pm->check_level().is_ok());

    auto sub = pm->subscribe([](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(sub.is_ok());
    TEST_ASSERT_TRUE(pm->unsubscribe(sub.value()).is_ok());
    TEST_ASSERT_TRUE(pm->deinit().is_ok());
}
