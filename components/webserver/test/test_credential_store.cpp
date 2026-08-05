/**
 * @file test_credential_store.cpp
 * @brief Unit tests for CredentialStore (requires NVS → tagged [hw]).
 */

#include "unity.h"
#include "credential_store.hpp"

TEST_CASE("ct_equal: constant-time compare", "[creds]")
{
    const uint8_t a[4] = {1, 2, 3, 4};
    const uint8_t b[4] = {1, 2, 3, 4};
    const uint8_t c[4] = {1, 2, 3, 5};
    TEST_ASSERT_TRUE(fpc::ct_equal(a, b, 4));
    TEST_ASSERT_FALSE(fpc::ct_equal(a, c, 4));
}

TEST_CASE("CredentialStore: set then check pass/fail", "[creds][hw]")
{
    fpc::CredentialStore store;
    TEST_ASSERT_TRUE(store.init().is_ok());
    TEST_ASSERT_TRUE(store.clear().is_ok());

    TEST_ASSERT_TRUE(store.set("admin", "s3cret").is_ok());
    TEST_ASSERT_TRUE(store.check("admin", "s3cret"));
    TEST_ASSERT_FALSE(store.check("admin", "wrong"));
    TEST_ASSERT_FALSE(store.check("other", "s3cret"));

    (void)store.clear();
    TEST_ASSERT_TRUE(store.deinit().is_ok());
}

TEST_CASE("CredentialStore: registered flag lifecycle", "[creds][hw]")
{
    fpc::CredentialStore store;
    TEST_ASSERT_TRUE(store.init().is_ok());
    TEST_ASSERT_TRUE(store.clear().is_ok());

    auto before = store.is_registered();
    TEST_ASSERT_TRUE(before.is_ok());
    TEST_ASSERT_FALSE(before.value());

    TEST_ASSERT_TRUE(store.set_registered().is_ok());
    auto after = store.is_registered();
    TEST_ASSERT_TRUE(after.is_ok());
    TEST_ASSERT_TRUE(after.value());

    (void)store.clear();
    TEST_ASSERT_TRUE(store.deinit().is_ok());
}
