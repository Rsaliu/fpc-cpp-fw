/**
 * @file test_rs485.cpp
 * @brief Unity tests for fpc::Rs485 using an injected MockUartDriver.
 *
 * No real UART hardware is needed — all ESP-IDF uart_* calls are intercepted
 * by the MockUartDriver.
 *
 * Test coverage:
 *   §1  MockUartDriver helper class
 *   §2  Construction / get_config
 *   §3  init() — success, invalid params, invalid pins, GPIO failure, double-init
 *   §4  deinit() — success, not-initialised guard, RAII destructor
 *   §5  is_active() state
 *   §6  write() — success, not-initialised guard
 *   §7  read() — success, not-initialised guard
 *   §8  send_receive() — success, empty-buffer guard, not-initialised guard
 */

#include "unity.h"
#include "rs485.hpp"
#include <array>
#include <cstdint>
#include <cstring>

using namespace fpc;

// ═══════════════════════════════════════════════════════════════════════════
// § 1  MockUartDriver
// ═══════════════════════════════════════════════════════════════════════════

class MockUartDriver final : public IUartDriver {
public:
    // Call counters
    int param_config_count{0};
    int set_pin_count{0};
    int driver_install_count{0};
    int set_mode_count{0};
    int driver_delete_count{0};
    int write_bytes_count{0};
    int wait_tx_count{0};
    int read_bytes_count{0};

    // Configurable return codes
    esp_err_t param_config_ret{ESP_OK};
    esp_err_t set_pin_ret{ESP_OK};
    esp_err_t driver_install_ret{ESP_OK};
    esp_err_t set_mode_ret{ESP_OK};
    int       write_bytes_ret{0};  // bytes "written"
    int       read_bytes_ret{0};   // bytes "read"

    // Fake receive data injected by tests
    std::array<uint8_t, 64> rx_data{};
    int                     rx_data_len{0};

    esp_err_t param_config(uart_port_t, const uart_config_t*) override
    {
        ++param_config_count;
        return param_config_ret;
    }
    esp_err_t set_pin(uart_port_t, int, int, int, int) override
    {
        ++set_pin_count;
        return set_pin_ret;
    }
    esp_err_t driver_install(uart_port_t, int, int, int, void*, int) override
    {
        ++driver_install_count;
        return driver_install_ret;
    }
    esp_err_t set_mode(uart_port_t, uart_mode_t) override
    {
        ++set_mode_count;
        return ESP_OK;
    }
    esp_err_t driver_delete(uart_port_t) override
    {
        return ESP_OK;
    }
    int write_bytes(uart_port_t, const void*, size_t size) override
    {
        ++write_bytes_count;
        write_bytes_ret = static_cast<int>(size);
        return write_bytes_ret;
    }
    esp_err_t wait_tx_done(uart_port_t, TickType_t) override
    {
        ++wait_tx_count;
        return ESP_OK;
    }
    int read_bytes(uart_port_t, void* buf, uint32_t length, TickType_t) override
    {
        ++read_bytes_count;
        const int n = (rx_data_len < static_cast<int>(length))
                      ? rx_data_len : static_cast<int>(length);
        if (n > 0) std::memcpy(buf, rx_data.data(), static_cast<std::size_t>(n));
        return n;
    }
};

// Convenience: default valid config
static Rs485Config make_valid_config()
{
    return Rs485Config{
        .uart_num  = UART_NUM_2,
        .tx_pin    = GPIO_NUM_17,
        .rx_pin    = GPIO_NUM_16,
        .dir_pin   = GPIO_NUM_5,
        .baud_rate = 9600,
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Construction / get_config
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485::get_config returns config passed at construction", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    const auto& cfg = bus.get_config();
    TEST_ASSERT_EQUAL_INT(static_cast<int>(UART_NUM_2), static_cast<int>(cfg.uart_num));
    TEST_ASSERT_EQUAL_INT(9600, cfg.baud_rate);
}

TEST_CASE("Rs485 is_active is false before init", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    TEST_ASSERT_FALSE(bus.is_active());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 3  init()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485::init succeeds with valid config", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    TEST_ASSERT_TRUE(bus.init().is_ok());
    TEST_ASSERT_TRUE(bus.is_active());
    TEST_ASSERT_EQUAL_INT(1, uart.param_config_count);
    TEST_ASSERT_EQUAL_INT(1, uart.set_pin_count);
    TEST_ASSERT_EQUAL_INT(1, uart.driver_install_count);
    TEST_ASSERT_EQUAL_INT(1, uart.set_mode_count);
}

TEST_CASE("Rs485::init returns InvalidParameter when baud_rate <= 0", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{Rs485Config{.uart_num=UART_NUM_2, .tx_pin=GPIO_NUM_17,
                          .rx_pin=GPIO_NUM_16, .dir_pin=GPIO_NUM_5,
                          .baud_rate=0}, uart};
    auto r = bus.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::init returns InvalidPinNumber when tx_pin is GPIO_NUM_NC", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{Rs485Config{.uart_num=UART_NUM_2, .tx_pin=GPIO_NUM_NC,
                          .rx_pin=GPIO_NUM_16, .dir_pin=GPIO_NUM_5,
                          .baud_rate=9600}, uart};
    auto r = bus.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidPinNumber),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::init returns Failed when param_config fails", "[rs485]")
{
    MockUartDriver uart;
    uart.param_config_ret = ESP_FAIL;
    Rs485 bus{make_valid_config(), uart};
    auto r = bus.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::Failed),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::init returns Failed when set_pin fails", "[rs485]")
{
    MockUartDriver uart;
    uart.set_pin_ret = ESP_FAIL;
    Rs485 bus{make_valid_config(), uart};
    auto r = bus.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::Failed),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::init returns Failed when driver_install fails", "[rs485]")
{
    MockUartDriver uart;
    uart.driver_install_ret = ESP_FAIL;
    Rs485 bus{make_valid_config(), uart};
    auto r = bus.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::Failed),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::init returns InvalidState when called twice", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    bus.init();
    auto r = bus.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 4  deinit()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485::deinit succeeds after init", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    bus.init();
    TEST_ASSERT_TRUE(bus.deinit().is_ok());
    TEST_ASSERT_FALSE(bus.is_active());
}

TEST_CASE("Rs485::deinit returns InvalidState when not initialised", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    auto r = bus.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 5  is_active()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485 is_active is true after init and false after deinit", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    TEST_ASSERT_FALSE(bus.is_active());
    bus.init();
    TEST_ASSERT_TRUE(bus.is_active());
    bus.deinit();
    TEST_ASSERT_FALSE(bus.is_active());
}

// ═══════════════════════════════════════════════════════════════════════════
// § 6  write()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485::write succeeds after init and calls write_bytes", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    bus.init();

    constexpr std::array<uint8_t, 4> data = {0x01u, 0x02u, 0x03u, 0x04u};
    auto r = bus.write(ByteView{data});
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_INT(1, uart.write_bytes_count);
    TEST_ASSERT_EQUAL_INT(1, uart.wait_tx_count);
}

TEST_CASE("Rs485::write returns InvalidState when not initialised", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    constexpr std::array<uint8_t, 2> data = {0xAAu, 0xBBu};
    auto r = bus.write(ByteView{data});
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 7  read()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485::read succeeds and returns injected bytes", "[rs485]")
{
    MockUartDriver uart;
    uart.rx_data[0] = 0xAAu;
    uart.rx_data[1] = 0xBBu;
    uart.rx_data_len = 2;

    Rs485 bus{make_valid_config(), uart};
    bus.init();

    std::array<uint8_t, 8> buf{};
    int32_t n{0};
    auto r = bus.read(MutableByteView{buf}, 100u, n);
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_HEX8(0xAAu, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBBu, buf[1]);
}

TEST_CASE("Rs485::read returns InvalidState when not initialised", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    std::array<uint8_t, 8> buf{};
    int32_t n{0};
    auto r = bus.read(MutableByteView{buf}, 100u, n);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}

// ═══════════════════════════════════════════════════════════════════════════
// § 8  send_receive()
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("Rs485::send_receive transmits request and receives response", "[rs485]")
{
    MockUartDriver uart;
    uart.rx_data[0] = 0x01u;
    uart.rx_data[1] = 0x03u;
    uart.rx_data_len = 2;

    Rs485 bus{make_valid_config(), uart};
    bus.init();

    constexpr std::array<uint8_t, 6> req = {0x01u,0x03u,0x01u,0x02u,0x00u,0x01u};
    std::array<uint8_t, 32> resp{};
    int32_t n{0};

    auto r = bus.send_receive(ByteView{req}, MutableByteView{resp}, 100u, n);
    TEST_ASSERT_TRUE(r.is_ok());
    TEST_ASSERT_EQUAL_INT(1, uart.write_bytes_count);
    TEST_ASSERT_EQUAL_INT(2, n);
}

TEST_CASE("Rs485::send_receive returns InvalidParameter for empty request", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    bus.init();

    std::array<uint8_t, 8> resp{};
    int32_t n{0};
    auto r = bus.send_receive(ByteView{}, MutableByteView{resp}, 100u, n);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::send_receive returns InvalidParameter for empty response buf", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};
    bus.init();

    constexpr std::array<uint8_t, 4> req = {0x01u,0x02u,0x03u,0x04u};
    int32_t n{0};
    auto r = bus.send_receive(ByteView{req}, MutableByteView{}, 100u, n);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidParameter),
                          static_cast<int>(r.error()));
}

TEST_CASE("Rs485::send_receive returns InvalidState when not initialised", "[rs485]")
{
    MockUartDriver uart;
    Rs485 bus{make_valid_config(), uart};

    constexpr std::array<uint8_t, 4> req = {0x01u,0x02u,0x03u,0x04u};
    std::array<uint8_t, 8> resp{};
    int32_t n{0};
    auto r = bus.send_receive(ByteView{req}, MutableByteView{resp}, 100u, n);
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(SystemError::InvalidState),
                          static_cast<int>(r.error()));
}
