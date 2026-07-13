#include "unity.h"
#include "file_handler.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_log.h"

static const char* TAG = "SPIFFS_INIT";

    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    void  initialize_spiffs() {
     // Register and mount the VFS SPIFFS file system
    esp_err_t reg = esp_vfs_spiffs_register(&conf);
   
    if (reg != ESP_OK) {
        if (reg == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (reg == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(reg));
        }
        return;
    }

    // Verify SPIFFS mounting and query space info
    size_t total = 0, used = 0;
    reg = esp_spiffs_info(conf.partition_label, &total, &used);
    if (reg == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d bytes, used: %d bytes", total, used);
    }
    }
static fpc::FileHandlerConfig make_no_op_config()
{
    fpc::FileHandlerConfig cfg;
    cfg.init_fn   = []() { return fpc::Result<void>::ok(); };
    cfg.deinit_fn = []() { return fpc::Result<void>::ok(); };
    return cfg;
}

TEST_CASE("FileHandler: init with no-op callbacks succeeds", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());
    TEST_ASSERT_TRUE(fh.is_initialized());
}

TEST_CASE("FileHandler: deinit without init returns InvalidState", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    auto r = fh.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
}

TEST_CASE("FileHandler: init without callback still succeeds", "[file_handler]")
{
    fpc::FileHandlerConfig cfg;
    fpc::FileHandler fh{cfg};
    TEST_ASSERT_TRUE(fh.init().is_ok());
}

TEST_CASE("FileHandler: failing init_fn propagates error", "[file_handler]")
{
    fpc::FileHandlerConfig cfg;
    cfg.init_fn = []() { return fpc::Result<void>::err(fpc::SystemError::Failed); };
    fpc::FileHandler fh{cfg};
    TEST_ASSERT_TRUE(fh.init().is_err());
    TEST_ASSERT_FALSE(fh.is_initialized());
}

TEST_CASE("FileHandler: write empty path returns InvalidParameter", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());
    auto r = fh.write("", "data");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("FileHandler: read empty path returns InvalidParameter", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());
    auto r = fh.read("");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("FileHandler: append empty data returns InvalidParameter", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());
    auto r = fh.append("/test/file.txt", "");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("FileHandler: remove empty path returns InvalidParameter", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());
    auto r = fh.remove("");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("FileHandler: get_size empty path returns InvalidParameter", "[file_handler]")
{
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());
    auto r = fh.get_size("");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("FileHandler: write then read back via POSIX VFS", "[file_handler]")
{
    initialize_spiffs();
    fpc::FileHandler fh{make_no_op_config()};
    TEST_ASSERT_TRUE(fh.init().is_ok());

    char path[64];
    std::snprintf(path, sizeof(path), "/spiffs/fh_test_%u.txt",
                  (unsigned)xTaskGetTickCount());

    const std::string content = "hello from FileHandler";
    TEST_ASSERT_TRUE(fh.write(path, content).is_ok());

    auto r = fh.read(path);
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_STRING(content.c_str(), r.value().c_str());

    (void)fh.remove(path);
}
