#include "unity.h"
#include "subscriber_event_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class MockTankMonitor final : public fpc::ITankMonitor {
public:
    bool    fail_subscribe{false};
    int     subscribe_count{0};
    int     unsubscribe_count{0};
    int32_t next_id{42};

    fpc::Result<void>     init()     override { return fpc::Result<void>::ok(); }
    fpc::Result<void>     deinit()   override { return fpc::Result<void>::ok(); }
    int32_t               id()   const noexcept override { return 1; }
    fpc::TankMonitorState state() const noexcept override
        { return fpc::TankMonitorState::Initialized; }
    fpc::Result<void>     check_level() override { return fpc::Result<void>::ok(); }

    fpc::Result<int32_t> subscribe(fpc::TankMonitorEventCallback) override {
        ++subscribe_count;
        if (fail_subscribe) return fpc::Result<int32_t>::err(fpc::SystemError::Failed);
        return fpc::Result<int32_t>::ok(next_id);
    }
    fpc::Result<void> unsubscribe(int32_t) override {
        ++unsubscribe_count;
        return fpc::Result<void>::ok();
    }
};

static fpc::SubscriberEventTaskConfig make_cfg(MockTankMonitor* m)
{
    fpc::SubscriberEventTaskConfig cfg;
    cfg.monitor         = m;
    cfg.event_callback  = [](fpc::EventType, int32_t) {};
    cfg.log_interval_ms = 200;
    cfg.stack_size      = 4096;
    cfg.priority        = 5;
    return cfg;
}

TEST_CASE("SubscriberEventTask: null monitor returns NullParameter", "[subscriber_event_task]")
{
    fpc::SubscriberEventTaskConfig cfg;
    cfg.event_callback = [](fpc::EventType, int32_t) {};
    fpc::SubscriberEventTask task{cfg};
    TEST_ASSERT_TRUE(task.start().is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)task.start().error());
}

TEST_CASE("SubscriberEventTask: empty callback returns NullParameter", "[subscriber_event_task]")
{
    MockTankMonitor m;
    fpc::SubscriberEventTaskConfig cfg;
    cfg.monitor = &m;
    fpc::SubscriberEventTask task{cfg};
    TEST_ASSERT_TRUE(task.start().is_err());
}

TEST_CASE("SubscriberEventTask: stop without start returns InvalidState", "[subscriber_event_task]")
{
    MockTankMonitor m;
    fpc::SubscriberEventTask task{make_cfg(&m)};
    TEST_ASSERT_TRUE(task.stop().is_err());
}

TEST_CASE("SubscriberEventTask: start calls subscribe", "[subscriber_event_task]")
{
    MockTankMonitor m;
    fpc::SubscriberEventTask task{make_cfg(&m)};
    TEST_ASSERT_TRUE(task.start().is_ok());
    TEST_ASSERT_EQUAL_INT(1, m.subscribe_count);
    TEST_ASSERT_TRUE(task.is_running());
    TEST_ASSERT_TRUE(task.stop().is_ok());
    TEST_ASSERT_EQUAL_INT(1, m.unsubscribe_count);
}

TEST_CASE("SubscriberEventTask: failed subscribe propagates error", "[subscriber_event_task]")
{
    MockTankMonitor m;
    m.fail_subscribe = true;
    fpc::SubscriberEventTask task{make_cfg(&m)};
    TEST_ASSERT_TRUE(task.start().is_err());
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("SubscriberEventTask: double start returns InvalidState", "[subscriber_event_task]")
{
    MockTankMonitor m;
    fpc::SubscriberEventTask task{make_cfg(&m)};
    TEST_ASSERT_TRUE(task.start().is_ok());
    auto r = task.start();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
    (void)task.stop();
}

TEST_CASE("SubscriberEventTask: stop calls unsubscribe", "[subscriber_event_task]")
{
    MockTankMonitor m;
    fpc::SubscriberEventTask task{make_cfg(&m)};
    TEST_ASSERT_TRUE(task.start().is_ok());
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_TRUE(task.stop().is_ok());
    TEST_ASSERT_EQUAL_INT(1, m.unsubscribe_count);
    TEST_ASSERT_FALSE(task.is_running());
}
