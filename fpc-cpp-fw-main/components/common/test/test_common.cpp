/**
 * @file test_common.cpp
 * @brief Unity tests for the fpc::common component.
 *
 * Tests cover:
 *   - SystemError → string conversion (to_string)
 *   - Result<T>   normal operation (ok / err / value / value_or)
 *   - Result<void> specialisation
 *   - Boundary / edge cases (unknown enum cast, value_or on success)
 *
 * No hardware mocking required — all logic is pure compile-time / run-time
 * arithmetic with no peripheral access.
 */

#include "unity.h"
#include "common.hpp"

using namespace fpc;

// ═══════════════════════════════════════════════════════════════════════════
// § 1  SystemError to_string()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("to_string returns 'Ok' for SystemError::Ok", "[common]")
{
    TEST_ASSERT_EQUAL_STRING("Ok", to_string(SystemError::Ok).data());
}

TEST_CASE("to_string returns 'NullParameter' for SystemError::NullParameter", "[common]")
{
    TEST_ASSERT_EQUAL_STRING("NullParameter",
                             to_string(SystemError::NullParameter).data());
}

TEST_CASE("to_string returns 'TimedOut' for SystemError::TimedOut", "[common]")
{
    TEST_ASSERT_EQUAL_STRING("TimedOut",
                             to_string(SystemError::TimedOut).data());
}

TEST_CASE("to_string returns 'ChecksumValidationFailed'", "[common]")
{
    TEST_ASSERT_EQUAL_STRING("ChecksumValidationFailed",
                             to_string(SystemError::ChecksumValidationFailed).data());
}

TEST_CASE("to_string returns 'OutOfRange' for SystemError::OutOfRange", "[common]")
{
    TEST_ASSERT_EQUAL_STRING("OutOfRange",
                             to_string(SystemError::OutOfRange).data());
}

TEST_CASE("to_string returns 'UnknownError' for an out-of-range cast", "[common]")
{
    // Cast an arbitrary integer that has no named enumerator.
    auto unknown_err = static_cast<SystemError>(0xFFu);
    TEST_ASSERT_EQUAL_STRING("UnknownError", to_string(unknown_err).data());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Result<int>
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Result<int>::ok() is_ok is true, is_err is false", "[common]")
{
    auto r = Result<int>::ok(42);
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_FALSE(r.is_err());
}

TEST_CASE("Result<int>::ok() stores the correct value", "[common]")
{
    auto r = Result<int>::ok(99);
    TEST_ASSERT_EQUAL_INT(99, r.value());
}

TEST_CASE("Result<int>::err() is_err is true, is_ok is false", "[common]")
{
    auto r = Result<int>::err(SystemError::TimedOut);
    TEST_ASSERT_FALSE(r.is_ok());
    TEST_ASSERT_TRUE(r.is_err());
}

TEST_CASE("Result<int>::err() stores the correct error code", "[common]")
{
    auto r = Result<int>::err(SystemError::TimedOut);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::TimedOut),
                          static_cast<int>(r.error()));
}

TEST_CASE("Result<int>::value_or returns value when ok", "[common]")
{
    auto r = Result<int>::ok(7);
    TEST_ASSERT_EQUAL_INT(7, r.value_or(-1));
}

TEST_CASE("Result<int>::value_or returns fallback when err", "[common]")
{
    auto r = Result<int>::err(SystemError::Failed);
    TEST_ASSERT_EQUAL_INT(-1, r.value_or(-1));
}

TEST_CASE("Result<int>::value_or returns 0 fallback correctly", "[common]")
{
    auto r = Result<int>::err(SystemError::Busy);
    TEST_ASSERT_EQUAL_INT(0, r.value_or(0));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  Result<void>
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Result<void>::ok() is_ok is true", "[common]")
{
    auto r = Result<void>::ok();
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_FALSE(r.is_err());
}

TEST_CASE("Result<void>::err() is_err is true", "[common]")
{
    auto r = Result<void>::err(SystemError::NullParameter);
    TEST_ASSERT_FALSE(r.is_ok());
    TEST_ASSERT_TRUE(r.is_err());
}

TEST_CASE("Result<void>::err() stores correct error code", "[common]")
{
    auto r = Result<void>::err(SystemError::OperationFailed);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::OperationFailed),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  Type-alias / constant sanity checks
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kMaxFramePayloadLength is 256", "[common]")
{
    TEST_ASSERT_EQUAL_UINT(256U, fpc::kMaxFramePayloadLength);
}

TEST_CASE("kMaxUartBufferSize is 512", "[common]")
{
    TEST_ASSERT_EQUAL_UINT(512U, fpc::kMaxUartBufferSize);
}

TEST_CASE("Bytes type alias is usable as std::vector<uint8_t>", "[common]")
{
    fpc::Bytes buf = {0x01u, 0x02u, 0x03u};
    TEST_ASSERT_EQUAL_UINT(3U, buf.size());
    TEST_ASSERT_EQUAL_UINT(0x02u, buf[1]);
}

TEST_CASE("ByteView spans over a Bytes buffer correctly", "[common]")
{
    fpc::Bytes buf = {0xAAu, 0xBBu, 0xCCu};
    fpc::ByteView view{buf};
    TEST_ASSERT_EQUAL_UINT(3U, view.size());
    TEST_ASSERT_EQUAL_UINT(0xBBu, view[1]);
}
