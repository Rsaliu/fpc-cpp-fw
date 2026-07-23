#include "unity.h"
#include "webserver_task.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static fpc::WebserverTaskConfig make_cfg()
{
    fpc::WebserverTaskConfig cfg;
    cfg.webserver_config.port           = 80;
    cfg.webserver_config.max_connections = 4;
    cfg.webserver_config.base_path      = "/www";
    cfg.stack_size = 8192; cfg.priority = 5;
    return cfg;
}

TEST_CASE("WebserverTask: not running after construction", "[webserver_task]")
{
    fpc::WebserverTask task{make_cfg()};
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("WebserverTask: stop without start returns InvalidState", "[webserver_task]")
{
    fpc::WebserverTask task{make_cfg()};
    TEST_ASSERT_TRUE(task.stop().is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)task.stop().error());
}

TEST_CASE("WebserverTask: double start returns InvalidState", "[webserver_task]")
{
    fpc::WebserverTask task{make_cfg()};
    TEST_ASSERT_TRUE(task.start().is_ok());
    auto r = task.start();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
    vTaskDelay(pdMS_TO_TICKS(200));
    (void)task.stop();
}

TEST_CASE("WebserverTask: start sets is_running", "[webserver_task]")
{
    fpc::WebserverTask task{make_cfg()};
    TEST_ASSERT_TRUE(task.start().is_ok());
    TEST_ASSERT_TRUE(task.is_running());
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_TRUE(task.stop().is_ok());
    TEST_ASSERT_FALSE(task.is_running());
}

TEST_CASE("WebserverTask: setup_fn invoked after start", "[webserver_task]")
{
    bool setup_called = false;
    fpc::WebserverTaskConfig cfg = make_cfg();
    cfg.setup_fn = [&setup_called](fpc::IWebServer&) -> fpc::Result<void> {
        setup_called = true;
        return fpc::Result<void>::ok();
    };
    fpc::WebserverTask task{cfg};
    TEST_ASSERT_TRUE(task.start().is_ok());
    vTaskDelay(pdMS_TO_TICKS(500));
    TEST_ASSERT_TRUE(setup_called);
    (void)task.stop();
}
