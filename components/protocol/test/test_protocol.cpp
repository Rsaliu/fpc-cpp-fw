/**
 * @file test_protocol.cpp
 * @brief Unity tests for fpc::protocol::gl_a01.
 *
 * All test vectors are cross-checked against the reference C implementation
 * and against hand-computed CRC-16/MODBUS values.
 *
 * Test coverage:
 *   §1  build_write_address — known vector, CRC bytes, byte-for-byte compare.
 *   §2  build_read_level    — known vector, CRC bytes, payload size.
 *   §3  build_read_temp     — known vector (same as crc component vector).
 *   §4  interpret_response  — valid frame (level=754), CRC mismatch,
 *                             frame too short, different sensor values.
 *   §5  Constants sanity    — register addresses and sizes.
 *
 * No hardware mocking needed — pure frame math.
 */

#include "unity.h"
#include "protocol.hpp"
#include <cstring>
#include <array>

using namespace fpc;
using namespace fpc::protocol::gl_a01;

// ═══════════════════════════════════════════════════════════════════════════
// § 1  build_write_address
//
//  Reference vector (from test_protocol.c):
//    build_write_address(0x01, 0x05) == {0x01,0x06,0x02,0x00,0x00,0x05,0x48,0x71}
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("build_write_address: byte-for-byte matches reference vector", "[protocol]")
{
    constexpr std::array<uint8_t, 8> expected = {
        0x01u, 0x06u, 0x02u, 0x00u, 0x00u, 0x05u, 0x48u, 0x71u};

    const auto frame = build_write_address(0x01u, 0x05u);

    TEST_ASSERT_EQUAL_INT(0, std::memcmp(expected.data(), frame.data(), 8u));
}

TEST_CASE("build_write_address: function code is 0x06", "[protocol]")
{
    const auto frame = build_write_address(0x01u, 0x05u);
    TEST_ASSERT_EQUAL_HEX8(kFcWriteSingle, frame[1]);
}

TEST_CASE("build_write_address: new address is in byte[5]", "[protocol]")
{
    const auto frame = build_write_address(0x01u, 0x07u);
    TEST_ASSERT_EQUAL_HEX8(0x07u, frame[5]);
}

TEST_CASE("build_write_address: slave address is in byte[0]", "[protocol]")
{
    const auto frame = build_write_address(0x03u, 0x05u);
    TEST_ASSERT_EQUAL_HEX8(0x03u, frame[0]);
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  build_read_level
//
//  Reference vector:
//    build_read_level(0x01) == {0x01,0x03,0x01,0x00,0x00,0x01,0x85,0xF6}
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("build_read_level: byte-for-byte matches reference vector", "[protocol]")
{
    constexpr std::array<uint8_t, 8> expected = {
        0x01u, 0x03u, 0x01u, 0x00u, 0x00u, 0x01u, 0x85u, 0xF6u};

    const auto frame = build_read_level(0x01u);
    TEST_ASSERT_EQUAL_INT(0, std::memcmp(expected.data(), frame.data(), 8u));
}

TEST_CASE("build_read_level: function code is 0x03", "[protocol]")
{
    const auto frame = build_read_level(0x01u);
    TEST_ASSERT_EQUAL_HEX8(kFcReadHolding, frame[1]);
}

TEST_CASE("build_read_level: register address bytes are 0x01 0x00", "[protocol]")
{
    const auto frame = build_read_level(0x01u);
    TEST_ASSERT_EQUAL_HEX8(0x01u, frame[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00u, frame[3]);
}

TEST_CASE("build_read_level: count bytes are 0x00 0x01", "[protocol]")
{
    const auto frame = build_read_level(0x01u);
    TEST_ASSERT_EQUAL_HEX8(0x00u, frame[4]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, frame[5]);
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  build_read_temp
//
//  Reference vector (also used in crc tests):
//    build_read_temp(0x01) == {0x01,0x03,0x01,0x02,0x00,0x01,0x24,0x36}
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("build_read_temp: byte-for-byte matches reference vector", "[protocol]")
{
    constexpr std::array<uint8_t, 8> expected = {
        0x01u, 0x03u, 0x01u, 0x02u, 0x00u, 0x01u, 0x24u, 0x36u};

    const auto frame = build_read_temp(0x01u);
    TEST_ASSERT_EQUAL_INT(0, std::memcmp(expected.data(), frame.data(), 8u));
}

TEST_CASE("build_read_temp: register address bytes are 0x01 0x02", "[protocol]")
{
    const auto frame = build_read_temp(0x01u);
    TEST_ASSERT_EQUAL_HEX8(0x01u, frame[2]);
    TEST_ASSERT_EQUAL_HEX8(0x02u, frame[3]);
}

TEST_CASE("build_read_temp CRC lo=0x24, hi=0x36", "[protocol]")
{
    const auto frame = build_read_temp(0x01u);
    TEST_ASSERT_EQUAL_HEX8(0x24u, frame[6]);
    TEST_ASSERT_EQUAL_HEX8(0x36u, frame[7]);
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  interpret_response
//
//  Reference vector:
//    {0x01, 0x03, 0x02, 0x02, 0xF2, 0x38, 0xA1}  → sensor_data = 754 (0x02F2)
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("interpret_response: known vector gives sensor_data=754", "[protocol]")
{
    constexpr std::array<uint8_t, 7> frame = {
        0x01u, 0x03u, 0x02u, 0x02u, 0xF2u, 0x38u, 0xA1u};

    auto r = interpret_response(ByteView{frame.data(), frame.size()});
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_UINT16(754u, r.value());
}

TEST_CASE("interpret_response: level 0x0000 (empty tank)", "[protocol]")
{
    // Build a valid 7-byte frame with sensor value = 0.
    // Payload = {0x01, 0x03, 0x02, 0x00, 0x00}
    // CRC({0x01,0x03,0x02,0x00,0x00}) — compute inline using fpc::crc
    std::array<uint8_t, 7> frame = {0x01u, 0x03u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u};
    const uint16_t c = fpc::crc::crc16_modbus(ByteView{frame.data(), 5u});
    frame[5] = static_cast<uint8_t>(c & 0xFFu);
    frame[6] = static_cast<uint8_t>((c >> 8u) & 0xFFu);

    auto r = interpret_response(ByteView{frame.data(), frame.size()});
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_UINT16(0u, r.value());
}

TEST_CASE("interpret_response: returns ChecksumValidationFailed on CRC mismatch", "[protocol]")
{
    std::array<uint8_t, 7> frame = {
        0x01u, 0x03u, 0x02u, 0x02u, 0xF2u, 0x00u, 0x00u}; // bad CRC bytes
    auto r = interpret_response(ByteView{frame.data(), frame.size()});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::ChecksumValidationFailed),
                          static_cast<int>(r.error()));
}

TEST_CASE("interpret_response: corrupting one CRC byte causes failure", "[protocol]")
{
    std::array<uint8_t, 7> frame = {
        0x01u, 0x03u, 0x02u, 0x02u, 0xF2u, 0x38u, 0xA1u};
    frame[6] ^= 0xFFu;  // flip hi CRC byte
    auto r = interpret_response(ByteView{frame.data(), frame.size()});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::ChecksumValidationFailed),
                          static_cast<int>(r.error()));
}

TEST_CASE("interpret_response: returns InvalidLength for frame < 7 bytes", "[protocol]")
{
    constexpr std::array<uint8_t, 6> short_frame = {
        0x01u, 0x03u, 0x02u, 0x02u, 0xF2u, 0x38u};
    auto r = interpret_response(ByteView{short_frame.data(), short_frame.size()});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidLength),
                          static_cast<int>(r.error()));
}

TEST_CASE("interpret_response: returns InvalidLength for empty span", "[protocol]")
{
    auto r = interpret_response(ByteView{});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidLength),
                          static_cast<int>(r.error()));
}

TEST_CASE("interpret_response: larger frame still extracts bytes[3..4]", "[protocol]")
{
    // 9-byte frame: sensor value 0x01A4 = 420
    std::array<uint8_t, 9> frame = {
        0x01u, 0x03u, 0x04u, 0x01u, 0xA4u, 0x00u, 0x00u, 0x00u, 0x00u};
    // Compute CRC of first 7 bytes and set last 2.
    const uint16_t c = fpc::crc::crc16_modbus(ByteView{frame.data(), 7u});
    frame[7] = static_cast<uint8_t>(c & 0xFFu);
    frame[8] = static_cast<uint8_t>((c >> 8u) & 0xFFu);

    auto r = interpret_response(ByteView{frame.data(), frame.size()});
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_UINT16(420u, r.value());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  Constants
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("kRequestFrameSize == 8", "[protocol]")
{
    TEST_ASSERT_EQUAL_UINT(8u, kRequestFrameSize);
}

TEST_CASE("kResponseFrameSize == 7", "[protocol]")
{
    TEST_ASSERT_EQUAL_UINT(7u, kResponseFrameSize);
}

TEST_CASE("kRegLevel == 0x0100", "[protocol]")
{
    TEST_ASSERT_EQUAL_HEX16(0x0100u, kRegLevel);
}

TEST_CASE("kRegTemperature == 0x0102", "[protocol]")
{
    TEST_ASSERT_EQUAL_HEX16(0x0102u, kRegTemperature);
}
