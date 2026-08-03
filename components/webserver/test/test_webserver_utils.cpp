/**
 * @file test_webserver_utils.cpp
 * @brief Unit tests for the stateless webserver helper functions.
 */

#include "unity.h"
#include "webserver_utils.hpp"
#include <cstring>

TEST_CASE("parse_cookie: extracts named cookie", "[utils]")
{
    char out[65];
    auto r = fpc::parse_cookie("SID=abc123; Path=/", "SID", out, sizeof(out));
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_STRING("abc123", out);
}

TEST_CASE("parse_cookie: handles leading spaces and multiple cookies", "[utils]")
{
    char out[65];
    auto r = fpc::parse_cookie("foo=1; SID=xyz; bar=2", "SID", out, sizeof(out));
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_STRING("xyz", out);
}

TEST_CASE("parse_cookie: missing cookie returns error", "[utils]")
{
    char out[65];
    auto r = fpc::parse_cookie("foo=1; bar=2", "SID", out, sizeof(out));
    TEST_ASSERT_TRUE(r.is_err());
}

TEST_CASE("content_directory_name: maps extensions to folders", "[utils]")
{
    char buf[16];
    fpc::content_directory_name("/home_ui.html", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("html", buf);
    fpc::content_directory_name("/home_ui.js", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("js", buf);
    fpc::content_directory_name("/ui.css", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("css", buf);
    fpc::content_directory_name("/logo.png", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("img", buf);
    fpc::content_directory_name("/fav.ico", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("img", buf);
}

TEST_CASE("make_json_message: wraps message", "[utils]")
{
    char buf[64];
    const char* out = fpc::make_json_message("hello", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{ \"message\": \"hello\" }", out);
}
