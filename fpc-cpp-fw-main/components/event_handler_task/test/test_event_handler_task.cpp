#include "unity.h"
#include "event_handler_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

class MockRelay final : public fpc::IRelay {
public:
    int on_count{0}, off_count{0};
    bool fail_on{false}, fail_off{false};

    fpc::Result<void>            init()          override { return fpc::Result<void>::ok(); }
    fpc::Result<void>            deinit()        override { return fpc::Result<void>::ok(); }
    fpc::Result<fpc::RelayState> get_state() const override
        { return fpc::Result<fpc::RelayState>::ok(fpc::RelayState::Off); }
    fpc::RelayConfig             get_config() const override { return {0, GPIO_NUM_0}; }
    fpc::Result<void>            trip()          override { return fpc::Result<void>::ok(); }
    fpc::Result<void>            reset()         override { return fpc::Result<void>::ok(); }
    fpc::Result<void>            reset_and_on()  override { return fpc::Result<void>::ok(); }

    fpc::Result<void> on() override {
        ++on_count;
        return fail_on ? fpc::Result<void>::err(fpc::SystemError::Failed)
                       : fpc::Result<void>::ok();
    }
    fpc::Result<void> off() override {
        ++off_count;
        return fail_off ? fpc::Result<void>::err(fpc::SystemError::Failed)
                        : fpc::Result<void>::ok();
    }
};

static fpc::EventHandlerTaskConfig make_cfg(MockRelay* relay, QueueHandle_t q)
{
    fpc::EventHandlerTaskConfig cfg;
    cfg.relay = relay; cfg.event_queue = q;
    cfg.queue_wait_ms = 100; cfg.stack_size = 4096; cfg.priority = 5;
    return cfg;
}

TEST_CASE("EventHandlerTask: null relay returns NullParameter", "[event_handler_task]")
{
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::TankEvent));
    fpc::EventHandlerTask task{make_cfg(nullptr, q)};
    TEST_ASSERT_TRUE(task.start().is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)task.start().error());
    vQueueDelete(q);
}

TEST_CASE("EventHandlerTask: null queue returns NullParameter", "[event_handler_task]")
{
    MockRelay relay;
    fpc::EventHandlerTask task{make_cfg(&relay, nullptr)};
    auto r = task.start();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}

TEST_CASE("EventHandlerTask: stop without start returns InvalidState", "[event_handler_task]")
{
    MockRelay relay;
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::TankEvent));
    fpc::EventHandlerTask task{make_cfg(&relay, q)};
    TEST_ASSERT_TRUE(task.stop().is_err());
    vQueueDelete(q);
}

TEST_CASE("EventHandlerTask: start and stop", "[event_handler_task]")
{
    MockRelay relay;
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::TankEvent));
    fpc::EventHandlerTask task{make_cfg(&relay, q)};
    TEST_ASSERT_TRUE(task.start().is_ok());
    TEST_ASSERT_TRUE(task.is_running());
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_TRUE(task.stop().is_ok());
    TEST_ASSERT_FALSE(task.is_running());
    vQueueDelete(q);
}

TEST_CASE("EventHandlerTask: double start returns InvalidState", "[event_handler_task]")
{
    MockRelay relay;
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::TankEvent));
    fpc::EventHandlerTask task{make_cfg(&relay, q)};
    TEST_ASSERT_TRUE(task.start().is_ok());
    auto r = task.start();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
    task.stop();
    vQueueDelete(q);
}

TEST_CASE("EventHandlerTask: TankFull calls relay off()", "[event_handler_task]")
{
    MockRelay relay;
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::TankEvent));
    fpc::EventHandlerTask task{make_cfg(&relay, q)};
    task.start();
    fpc::TankEvent ev{fpc::EventType::TankFull};
    xQueueSendToBack(q, &ev, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_GREATER_OR_EQUAL(1, relay.off_count);
    task.stop();
    vQueueDelete(q);
}

TEST_CASE("EventHandlerTask: TankLow calls relay on()", "[event_handler_task]")
{
    MockRelay relay;
    QueueHandle_t q = xQueueCreate(4, sizeof(fpc::TankEvent));
    fpc::EventHandlerTask task{make_cfg(&relay, q)};
    task.start();
    fpc::TankEvent ev{fpc::EventType::TankLow};
    xQueueSendToBack(q, &ev, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_GREATER_OR_EQUAL(1, relay.on_count);
    task.stop();
    vQueueDelete(q);
}
