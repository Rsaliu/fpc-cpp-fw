#include "unity.h"
#include "setup_config_button.hpp"

static fpc::SetupConfigButtonConfig make_config(gpio_num_t pin = GPIO_NUM_16)
{
    fpc::SetupConfigButtonConfig cfg;
    cfg.button_pin   = pin;
    cfg.main_task_cb = []() {};
    cfg.webserver_cb = []() {};
    return cfg;
}

TEST_CASE("SetupConfigButton: not initialized after construction", "[setup_config_button]")
{
    fpc::SetupConfigButton btn{make_config()};
    TEST_ASSERT_FALSE(btn.is_initialized());
}

TEST_CASE("SetupConfigButton: init with GPIO_NUM_NC returns InvalidParameter", "[setup_config_button]")
{
    fpc::SetupConfigButton btn{make_config(GPIO_NUM_NC)};
    auto r = btn.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("SetupConfigButton: null webserver_cb accepted at construction", "[setup_config_button]")
{
    fpc::SetupConfigButtonConfig cfg = make_config();
    cfg.webserver_cb = nullptr;
    fpc::SetupConfigButton btn{cfg};
    TEST_ASSERT_FALSE(btn.is_initialized());
}

TEST_CASE("SetupConfigButton: init on valid pin succeeds", "[setup_config_button]")
{
    fpc::SetupConfigButton btn{make_config(GPIO_NUM_16)};
    auto r = btn.init();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_TRUE(btn.is_initialized());
}
