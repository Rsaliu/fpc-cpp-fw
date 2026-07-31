#include "unity.h"
#include "pump_control_unit.hpp"
#include "esp_log.h"

static const char* TAG = "test_pump_control_unit";

// ─── Mock monitors ────────────────────────────────────────────────────────────// NOTE: wrapped in an anonymous namespace — identically named mock classes
// exist in other test files; without internal linkage the linker merges
// their vtables (ODR violation) and virtual calls dispatch into the wrong
// file's implementation.
namespace {
struct MockPumpMonitor final : public fpc::IPumpMonitor {
    int32_t mid;
    int     check_count{0};
    bool    fail_check{false};

    explicit MockPumpMonitor(int32_t id) : mid(id) {}

    int32_t                      id()      const noexcept override { return mid; }
    fpc::Result<void>            init()    override { return fpc::Result<void>::ok(); }
    fpc::Result<void>            deinit()  override { return fpc::Result<void>::ok(); }
    fpc::PumpMonitorState        state()   const noexcept override
        { return fpc::PumpMonitorState::Initialized; }
    fpc::Result<int32_t>         subscribe(fpc::PumpMonitorEventCallback cb) override {
        (void)cb; return fpc::Result<int32_t>::ok(0);
    }
    fpc::Result<void>            unsubscribe(int32_t) override
        { return fpc::Result<void>::ok(); }

    fpc::Result<void> check_current() override {
        ++check_count;
        return fail_check
            ? fpc::Result<void>::err(fpc::SystemError::Failed)
            : fpc::Result<void>::ok();
    }
};

struct MockTankMonitor final : public fpc::ITankMonitor {
    int32_t mid;
    int     check_count{0};

    explicit MockTankMonitor(int32_t id) : mid(id) {}

    int32_t                    id()      const noexcept override { return mid; }
    fpc::Result<void>          init()    override { return fpc::Result<void>::ok(); }
    fpc::Result<void>          deinit()  override { return fpc::Result<void>::ok(); }
    fpc::TankMonitorState      state()   const noexcept override
        { return fpc::TankMonitorState::Initialized; }
    fpc::Result<int32_t>       subscribe(fpc::TankMonitorEventCallback cb) override {
        (void)cb; return fpc::Result<int32_t>::ok(0);
    }
    fpc::Result<void>          unsubscribe(int32_t) override
        { return fpc::Result<void>::ok(); }
    fpc::Result<void>          check_level() override {
        ++check_count;
        return fpc::Result<void>::ok();
    }
};

} // namespace

// ─── Lifecycle ────────────────────────────────────────────────────────────────

TEST_CASE("PumpControlUnit: not initialized on construction", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    TEST_ASSERT_FALSE(pcu.is_initialized());
}

TEST_CASE("PumpControlUnit: init succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    auto res = pcu.init();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_TRUE(pcu.is_initialized());
}

TEST_CASE("PumpControlUnit: double init returns InvalidState", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.init();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpControlUnit: deinit succeeds after init", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.deinit();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_FALSE(pcu.is_initialized());
}

TEST_CASE("PumpControlUnit: deinit fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    auto res = pcu.deinit();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

// ─── Pump-monitor registry ────────────────────────────────────────────────────

TEST_CASE("PumpControlUnit: add_pump_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    MockPumpMonitor pm{1};

    auto res = pcu.add_pump_monitor(pm);
    TEST_ASSERT_TRUE(res.is_ok());
}

TEST_CASE("PumpControlUnit: add_pump_monitor fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    MockPumpMonitor pm{1};
    auto res = pcu.add_pump_monitor(pm);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpControlUnit: add duplicate pump_monitor returns InvalidParameter", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    MockPumpMonitor pm{1};
    pcu.add_pump_monitor(pm);

    auto res = pcu.add_pump_monitor(pm);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("PumpControlUnit: remove_pump_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    MockPumpMonitor pm{1};
    pcu.add_pump_monitor(pm);

    auto res = pcu.remove_pump_monitor(1);
    TEST_ASSERT_TRUE(res.is_ok());
}

TEST_CASE("PumpControlUnit: remove non-existent pump_monitor returns InvalidParameter", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.remove_pump_monitor(99);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("PumpControlUnit: overflow at kMaxMonitors pump monitors", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    // Use a local array so each monitor has a unique address
    MockPumpMonitor mocks[fpc::PumpControlUnit::kMaxMonitors + 1] = {
        MockPumpMonitor{0},  MockPumpMonitor{1},  MockPumpMonitor{2},
        MockPumpMonitor{3},  MockPumpMonitor{4},  MockPumpMonitor{5},
        MockPumpMonitor{6},  MockPumpMonitor{7},  MockPumpMonitor{8},
        MockPumpMonitor{9},  MockPumpMonitor{10}
    };

    for (int32_t i = 0; i < fpc::PumpControlUnit::kMaxMonitors; ++i) {
        TEST_ASSERT_TRUE(pcu.add_pump_monitor(mocks[i]).is_ok());
    }
    auto overflow = pcu.add_pump_monitor(mocks[fpc::PumpControlUnit::kMaxMonitors]);
    TEST_ASSERT_TRUE(overflow.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::BufferOverflow, overflow.error());
}

// ─── Tank-monitor registry ────────────────────────────────────────────────────

TEST_CASE("PumpControlUnit: add_tank_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    MockTankMonitor tm{1};

    auto res = pcu.add_tank_monitor(tm);
    TEST_ASSERT_TRUE(res.is_ok());
}

TEST_CASE("PumpControlUnit: add_tank_monitor fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    MockTankMonitor tm{1};
    auto res = pcu.add_tank_monitor(tm);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpControlUnit: remove_tank_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    MockTankMonitor tm{1};
    pcu.add_tank_monitor(tm);

    auto res = pcu.remove_tank_monitor(1);
    TEST_ASSERT_TRUE(res.is_ok());
}

TEST_CASE("PumpControlUnit: remove non-existent tank_monitor returns InvalidParameter", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.remove_tank_monitor(99);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

// ─── Subscriber helpers ───────────────────────────────────────────────────────

TEST_CASE("PumpControlUnit: add_subscriber_to_pump_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    fpc::PumpConfig pump_cfg;
    pump_cfg.id                  = 1;
    pump_cfg.make                = "Mock";
    pump_cfg.current_rating      = 5.0f;
    pump_cfg.min_working_current = 0.5f;
    pump_cfg.power_hp            = 1.0f;

    fpc::PumpMonitorConfig pm_cfg;
    pm_cfg.id             = 1;
    pm_cfg.pump_config    = pump_cfg;
    pm_cfg.number_of_samples = 1;
    pm_cfg.read_cb        = []() -> fpc::Result<float> { return fpc::Result<float>::ok(3.0f); };
    pm_cfg.analytics_cb   = [](fpc::Span<const float>, float, float)
        -> fpc::PumpStateMachineState { return fpc::PumpStateMachineState::Normal; };

    fpc::PumpMonitor pm{pm_cfg};
    pm.init();
    pcu.add_pump_monitor(pm);

    fpc::EventType last_event = fpc::EventType::Unknown;
    auto res = pcu.add_subscriber_to_pump_monitor(
        1, [&last_event](fpc::EventType e, int32_t) { last_event = e; });
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_NOT_EQUAL(-1, res.value());
}

TEST_CASE("PumpControlUnit: add_subscriber fails for unknown pm_id", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.add_subscriber_to_pump_monitor(99, [](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("PumpControlUnit: remove_subscriber_from_pump_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    fpc::PumpConfig pump_cfg;
    pump_cfg.id = 1; pump_cfg.make = "Mock";
    pump_cfg.current_rating = 5.0f; pump_cfg.min_working_current = 0.5f;

    fpc::PumpMonitorConfig pm_cfg;
    pm_cfg.id = 1; pm_cfg.pump_config = pump_cfg; pm_cfg.number_of_samples = 1;
    pm_cfg.read_cb = []() -> fpc::Result<float> { return fpc::Result<float>::ok(3.0f); };
    pm_cfg.analytics_cb = [](fpc::Span<const float>, float, float)
        -> fpc::PumpStateMachineState { return fpc::PumpStateMachineState::Normal; };

    fpc::PumpMonitor pm{pm_cfg};
    pm.init();
    pcu.add_pump_monitor(pm);

    auto sub = pcu.add_subscriber_to_pump_monitor(1, [](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(sub.is_ok());

    auto unsub = pcu.remove_subscriber_from_pump_monitor(1, sub.value());
    TEST_ASSERT_TRUE(unsub.is_ok());
}

// ─── Tank-monitor subscriber helpers ─────────────────────────────────────────

TEST_CASE("PumpControlUnit: add_subscriber_to_tank_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    MockTankMonitor tm{1};
    pcu.add_tank_monitor(tm);

    auto res = pcu.add_subscriber_to_tank_monitor(
        1, [](fpc::EventType, int32_t) {});
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(0, res.value());  // MockTankMonitor::subscribe returns 0
}

TEST_CASE("PumpControlUnit: add_subscriber_to_tank_monitor fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    auto res = pcu.add_subscriber_to_tank_monitor(1, [](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpControlUnit: add_subscriber_to_tank_monitor fails for unknown id", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.add_subscriber_to_tank_monitor(99, [](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("PumpControlUnit: remove_subscriber_from_tank_monitor succeeds", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    MockTankMonitor tm{1};
    pcu.add_tank_monitor(tm);

    auto sub = pcu.add_subscriber_to_tank_monitor(1, [](fpc::EventType, int32_t){});
    TEST_ASSERT_TRUE(sub.is_ok());

    auto unsub = pcu.remove_subscriber_from_tank_monitor(1, sub.value());
    TEST_ASSERT_TRUE(unsub.is_ok());
}

TEST_CASE("PumpControlUnit: remove_subscriber_from_tank_monitor fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    auto res = pcu.remove_subscriber_from_tank_monitor(1, 0);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpControlUnit: remove_subscriber_from_tank_monitor fails for unknown id", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    auto res = pcu.remove_subscriber_from_tank_monitor(99, 0);
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

// ─── Polling loops ────────────────────────────────────────────────────────────

TEST_CASE("PumpControlUnit: loop_pump_monitors calls check_current on all", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    MockPumpMonitor m1{1}, m2{2}, m3{3};
    pcu.add_pump_monitor(m1);
    pcu.add_pump_monitor(m2);
    pcu.add_pump_monitor(m3);

    auto res = pcu.loop_pump_monitors();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(1, m1.check_count);
    TEST_ASSERT_EQUAL(1, m2.check_count);
    TEST_ASSERT_EQUAL(1, m3.check_count);
}

TEST_CASE("PumpControlUnit: loop_pump_monitors fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    auto res = pcu.loop_pump_monitors();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpControlUnit: loop_pump_monitors continues after check_current error", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();

    MockPumpMonitor m1{1}, m2{2};
    m1.fail_check = true;
    pcu.add_pump_monitor(m1);
    pcu.add_pump_monitor(m2);

    auto res = pcu.loop_pump_monitors();
    TEST_ASSERT_TRUE(res.is_ok());   // loop itself succeeds
    TEST_ASSERT_EQUAL(1, m1.check_count);
    TEST_ASSERT_EQUAL(1, m2.check_count);
}

TEST_CASE("PumpControlUnit: loop_level_monitors calls check_level on all", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    pcu.init();
    MockTankMonitor t1{1}, t2{2};
    pcu.add_tank_monitor(t1);
    pcu.add_tank_monitor(t2);
    auto res = pcu.loop_level_monitors();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_EQUAL(1, t1.check_count);
    TEST_ASSERT_EQUAL(1, t2.check_count);
    ESP_LOGI(TAG, "t1=%d t2=%d", t1.check_count, t2.check_count);
}

TEST_CASE("PumpControlUnit: loop_level_monitors fails when not initialized", "[pump_control_unit]")
{
    fpc::PumpControlUnit pcu;
    auto res = pcu.loop_level_monitors();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}
