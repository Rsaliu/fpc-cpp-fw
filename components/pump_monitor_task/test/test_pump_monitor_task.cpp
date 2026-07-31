#include "unity.h"
#include "pump_monitor_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "test_pump_monitor_task";

// ─── Mock IPumpMonitor ────────────────────────────────────────────────────────

/// Minimal mock: counts check_current() invocations.
/// Optionally injects a failure after a threshold.
// Anonymous namespace: prevents ODR clashes with same-named mocks in other tests.
namespace {
class MockPumpMonitor final : public fpc::IPumpMonitor {
public:
    int  check_count{0};
    bool fail_check{false};

    fpc::Result<void>            init()    override { return fpc::Result<void>::ok(); }
    fpc::Result<void>            deinit()  override { return fpc::Result<void>::ok(); }
    int32_t                      id()      const noexcept override { return 1; }
    fpc::PumpMonitorState        state()   const noexcept override
        { return fpc::PumpMonitorState::Initialized; }
    fpc::Result<int32_t>         subscribe(fpc::PumpMonitorEventCallback) override
        { return fpc::Result<int32_t>::ok(0); }
    fpc::Result<void>            unsubscribe(int32_t) override
        { return fpc::Result<void>::ok(); }

    fpc::Result<void> check_current() override {
        ++check_count;
        if (fail_check) {
            return fpc::Result<void>::err(fpc::SystemError::Failed);
        }
        return fpc::Result<void>::ok();
    }
};
} // namespace

// ─── Helpers ──────────────────────────────────────────────────────────────────

static fpc::PumpMonitorTaskConfig make_cfg(MockPumpMonitor* mock,
                                           uint32_t interval_ms = 100)
{
    fpc::PumpMonitorTaskConfig cfg;
    cfg.id                = 1;
    cfg.monitor           = mock;
    cfg.check_interval_ms = interval_ms;
    cfg.stack_size        = 4096;
    cfg.priority          = 5;
    return cfg;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

TEST_CASE("PumpMonitorTask: not running before start", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    fpc::PumpMonitorTask task{make_cfg(&mock)};
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("PumpMonitorTask: start sets is_running", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    fpc::PumpMonitorTask task{make_cfg(&mock)};

    auto res = task.start();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_TRUE(task.is_running());

    task.stop();
}

TEST_CASE("PumpMonitorTask: stop clears is_running", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    fpc::PumpMonitorTask task{make_cfg(&mock)};

    task.start();
    auto res = task.stop();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("PumpMonitorTask: start fails when already running", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    fpc::PumpMonitorTask task{make_cfg(&mock)};

    task.start();
    auto res = task.start();   // second start must fail
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());

    task.stop();
}

TEST_CASE("PumpMonitorTask: stop fails when not running", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    fpc::PumpMonitorTask task{make_cfg(&mock)};

    auto res = task.stop();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

TEST_CASE("PumpMonitorTask: start fails with null monitor", "[pump_monitor_task]")
{
    fpc::PumpMonitorTaskConfig cfg;
    cfg.id      = 1;
    cfg.monitor = nullptr;

    fpc::PumpMonitorTask task{cfg};
    auto res = task.start();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::NullParameter, res.error());
}

// ─── check_current invocation ─────────────────────────────────────────────────

TEST_CASE("PumpMonitorTask: check_current called while running", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    // interval = 100 ms; run for ~350 ms → expect at least 2 calls
    fpc::PumpMonitorTask task{make_cfg(&mock, 100)};

    task.start();
    vTaskDelay(pdMS_TO_TICKS(350));
    task.stop();

    ESP_LOGI(TAG, "check_count = %d", mock.check_count);
    TEST_ASSERT_GREATER_OR_EQUAL(2, mock.check_count);
}

TEST_CASE("PumpMonitorTask: check_current NOT called after stop", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    fpc::PumpMonitorTask task{make_cfg(&mock, 100)};

    task.start();
    vTaskDelay(pdMS_TO_TICKS(200));
    task.stop();

    int count_at_stop = mock.check_count;
    vTaskDelay(pdMS_TO_TICKS(300));   // wait extra — count must not increase
    TEST_ASSERT_EQUAL(count_at_stop, mock.check_count);
}

TEST_CASE("PumpMonitorTask: continues running even when check_current fails",
          "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    mock.fail_check = true;  // every call returns an error

    fpc::PumpMonitorTask task{make_cfg(&mock, 100)};
    task.start();
    vTaskDelay(pdMS_TO_TICKS(350));
    task.stop();

    // Task must still have called check_current despite errors
    TEST_ASSERT_GREATER_OR_EQUAL(2, mock.check_count);
}

// ─── Destructor ───────────────────────────────────────────────────────────────

TEST_CASE("PumpMonitorTask: destructor stops running task", "[pump_monitor_task]")
{
    MockPumpMonitor mock;
    {
        fpc::PumpMonitorTask task{make_cfg(&mock, 100)};
        task.start();
        TEST_ASSERT_TRUE(task.is_running());
        // destructor called here — must not crash or deadlock
    }
    // If we reach here the destructor completed cleanly
    TEST_ASSERT_TRUE(true);
}
