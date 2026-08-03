/**
 * @file test_session_manager.cpp
 * @brief Unit tests for SessionManager.
 */

#include "unity.h"
#include "session_manager.hpp"
#include <cstring>

TEST_CASE("SessionManager: create then find by token and username", "[session][hw]")
{
    fpc::SessionManager mgr;
    fpc::Session* s = mgr.create("alice");
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s->used);
    TEST_ASSERT_EQUAL_STRING("alice", s->username.data());

    TEST_ASSERT_EQUAL_PTR(s, mgr.find_by_token(s->token.data()));
    TEST_ASSERT_EQUAL_PTR(s, mgr.find_by_username("alice"));
    mgr.clear();
}

TEST_CASE("SessionManager: unknown token/user returns null", "[session][hw]")
{
    fpc::SessionManager mgr;
    (void)mgr.create("bob");
    TEST_ASSERT_NULL(mgr.find_by_token("deadbeef"));
    TEST_ASSERT_NULL(mgr.find_by_username("nobody"));
    mgr.clear();
}

TEST_CASE("SessionManager: remove marks slot unused", "[session][hw]")
{
    fpc::SessionManager mgr;
    fpc::Session* s = mgr.create("carol");
    char tok[fpc::kSessionTokenLen + 1];
    std::strncpy(tok, s->token.data(), sizeof(tok));
    mgr.remove(s);
    TEST_ASSERT_NULL(mgr.find_by_token(tok));
    mgr.remove(nullptr); // null-safe
    mgr.clear();
}

TEST_CASE("SessionManager: tokens are unique", "[session][hw]")
{
    fpc::SessionManager mgr;
    fpc::Session* a = mgr.create("u1");
    fpc::Session* b = mgr.create("u2");
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_NOT_EQUAL(0, std::strncmp(a->token.data(), b->token.data(),
                                          fpc::kSessionTokenLen));
    mgr.clear();
}

TEST_CASE("SessionManager: clear empties all slots", "[session][hw]")
{
    fpc::SessionManager mgr;
    (void)mgr.create("x");
    mgr.clear();
    TEST_ASSERT_NULL(mgr.find_by_username("x"));
}
