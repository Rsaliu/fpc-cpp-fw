#include "unity.h"
#include "tank_monitor_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "test_tank_monitor_task";

// ─── Mock ITankMonitor ────────────────────────────────────────────────────────

/// Minimal mock: counts check_level() invocations.
class MockTankMonitor final : public fpc::ITankMonitor {
public:
    int  check_count{0};
    bool fail_check{false};

    fpc::Result<void>          init()    override { return fpc::Result<void>::ok(); }
    fpc::Result<void>          deinit()  override { return fpc::Result<void>::ok(); }
    int32_t                    id()      const noexcept override { return 1; }
    fpc::TankMonitorState      state()   const noexcept override
        { return fpc::TankMonitorState::Initialized; }
    fpc::Result<int32_t>       subscribe(fpc::TankMonitorEventCallback) override
        { return fpc::Result<int32_t>::ok(0); }
    fpc::Result<void>          unsubscribe(int32_t) override
        { return fpc::Result<void>::ok(); }

    fpc::Result<void> check_level() override {
        ++check_count;
        if (fail_check) {
            return fpc::Result<void>::err(fpc::SystemError::Failed);
        }
        return fpc::Result<void>::ok();
    }
};

// ─── Helpers ──────────────────────────────────────────────────────────────────

static fpc::TankMonitorTaskConfig make_cfg(
    std::vector<fpc::ITankMonitor*> monitors,
    uint32_t interval_ms = 100)
{
    fpc::TankMonitorTaskConfig cfg;
    cfg.id                = 1;
    cfg.monitors          = std::move(monitors);
    cfg.check_interval_ms = interval_ms;
    cfg.stack_size        = 4096;
    cfg.priority          = 5;
    return cfg;
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

TEST_CASE("TankMonitorTask: not running before start", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock})};
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("TankMonitorTask: start sets is_running", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock})};

    auto res = task.start();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_TRUE(task.is_running());

    task.stop();
}

TEST_CASE("TankMonitorTask: stop clears is_running", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock})};

    task.start();
    auto res = task.stop();
    TEST_ASSERT_TRUE(res.is_ok());
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("TankMonitorTask: start fails with empty monitors list", "[tank_monitor_task]")
{
    fpc::TankMonitorTask task{make_cfg({})};
    auto res = task.start();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidParameter, res.error());
}

TEST_CASE("TankMonitorTask: start fails when already running", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock})};

    task.start();
    auto res = task.start();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());

    task.stop();
}

TEST_CASE("TankMonitorTask: stop fails when not running", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock})};

    auto res = task.stop();
    TEST_ASSERT_TRUE(res.is_err());
    TEST_ASSERT_EQUAL(fpc::SystemError::InvalidState, res.error());
}

// ─── check_level invocation ───────────────────────────────────────────────────

TEST_CASE("TankMonitorTask: check_level called while running", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock}, 100)};

    task.start();
    vTaskDelay(pdMS_TO_TICKS(350));
    task.stop();

    ESP_LOGI(TAG, "check_count = %d", mock.check_count);
    TEST_ASSERT_GREATER_OR_EQUAL(2, mock.check_count);
}

TEST_CASE("TankMonitorTask: check_level NOT called after stop", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    fpc::TankMonitorTask task{make_cfg({&mock}, 100)};

    task.start();
    vTaskDelay(pdMS_TO_TICKS(200));
    task.stop();

    int count_at_stop = mock.check_count;
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_EQUAL(count_at_stop, mock.check_count);
}

TEST_CASE("TankMonitorTask: check_level called on all monitors", "[tank_monitor_task]")
{
    MockTankMonitor m1, m2, m3;
    fpc::TankMonitorTask task{make_cfg({&m1, &m2, &m3}, 100)};

    task.start();
    vTaskDelay(pdMS_TO_TICKS(350));
    task.stop();

    ESP_LOGI(TAG, "counts: m1=%d m2=%d m3=%d", m1.check_count, m2.check_count, m3.check_count);
    TEST_ASSERT_GREATER_OR_EQUAL(2, m1.check_count);
    TEST_ASSERT_GREATER_OR_EQUAL(2, m2.check_count);
    TEST_ASSERT_GREATER_OR_EQUAL(2, m3.check_count);
}

TEST_CASE("TankMonitorTask: continues running even when check_level fails",
          "[tank_monitor_task]")
{
    MockTankMonitor mock;
    mock.fail_check = true;

    fpc::TankMonitorTask task{make_cfg({&mock}, 100)};
    task.start();
    vTaskDelay(pdMS_TO_TICKS(350));
    task.stop();

    TEST_ASSERT_GREATER_OR_EQUAL(2, mock.check_count);
}

// ─── Destructor ───────────────────────────────────────────────────────────────

TEST_CASE("TankMonitorTask: destructor stops running task", "[tank_monitor_task]")
{
    MockTankMonitor mock;
    {
        fpc::TankMonitorTask task{make_cfg({&mock}, 100)};
        task.start();
        TEST_ASSERT_TRUE(task.is_running());
        // destructor called here — must not crash or deadlock
    }
    TEST_ASSERT_TRUE(true);
}
