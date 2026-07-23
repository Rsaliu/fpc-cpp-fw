#include "unity.h"
#include "utils.hpp"

TEST_CASE("swap_string: basic replacement", "[utils]")
{
    auto r = fpc::utils::swap_string("hello world", "world", "ESP32");
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_STRING("hello ESP32", r.value().c_str());
}

TEST_CASE("swap_string: replacement at start", "[utils]")
{
    auto r = fpc::utils::swap_string("firmware.bin", "firmware", "new_fw");
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_STRING("new_fw.bin", r.value().c_str());
}

TEST_CASE("swap_string: substring not found returns Failed", "[utils]")
{
    auto r = fpc::utils::swap_string("hello", "xyz", "abc");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::Failed, (int)r.error());
}

TEST_CASE("swap_string: empty to_swap returns InvalidParameter", "[utils]")
{
    auto r = fpc::utils::swap_string("hello", "", "abc");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("swap_string: replacement with empty string", "[utils]")
{
    auto r = fpc::utils::swap_string("hello world", "world", "");
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_STRING("hello ", r.value().c_str());
}

TEST_CASE("is_valid_json: valid simple object", "[utils]")
{
    auto r = fpc::utils::is_valid_json(R"({"key":"value"})");
    TEST_ASSERT_TRUE(r.is_ok());
}

TEST_CASE("is_valid_json: valid array", "[utils]")
{
    auto r = fpc::utils::is_valid_json(R"([1, 2, 3])");
    TEST_ASSERT_TRUE(r.is_ok());
}

TEST_CASE("is_valid_json: invalid json returns Failed", "[utils]")
{
    auto r = fpc::utils::is_valid_json("{not valid json}");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::Failed, (int)r.error());
}

TEST_CASE("is_valid_json: empty string returns InvalidParameter", "[utils]")
{
    auto r = fpc::utils::is_valid_json("");
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("is_valid_json: nested object", "[utils]")
{
    auto r = fpc::utils::is_valid_json(R"({"a":{"b":{"c":42}}})");
    TEST_ASSERT_TRUE(r.is_ok());
}

TEST_CASE("get_nvs_blob_size: null key returns NullParameter", "[utils]")
{
    auto r = fpc::utils::get_nvs_blob_size(0, nullptr);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}

TEST_CASE("get_nvs_blob: null key returns NullParameter", "[utils]")
{
    auto r = fpc::utils::get_nvs_blob(0, nullptr, 64);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}

TEST_CASE("get_nvs_blob: zero max_size returns NullParameter", "[utils]")
{
    auto r = fpc::utils::get_nvs_blob(0, "key", 0);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::NullParameter, (int)r.error());
}
