/**
 * @file rs485.hpp
 * @brief RAII RS-485 / UART driver — C++17 with injectable IUartDriver.
 *
 * Architecture (dependency injection):
 *
 *   IUartDriver  ← abstraction over ESP-IDF uart_* calls
 *       └─ EspUartDriver   (production)
 *       └─ MockUartDriver  (tests — records calls, returns configurable result)
 *
 *   Rs485        ← RAII class; owns Rs485Config, references IUartDriver.
 *                  Replaces both reference `rs485_t` and `rs485_context`.
 *
 * Key C++17 improvements over the reference:
 *  - No `void*` context pointer — `send_receive()` is a method on Rs485.
 *  - Receive callback uses `std::function<void(ByteView)>` instead of `void*`.
 *  - `ByteView` / `MutableByteView` replace raw char* + size_t pairs.
 *  - `Result<T>` replaces `error_type_t` out-parameters.
 *  - UART Rx buffer size is a `constexpr`, not a `#define`.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string_view>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "common.hpp"

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Constants
// ═══════════════════════════════════════════════════════════════════════════

/// Default UART Rx ring-buffer size (bytes).
inline constexpr std::size_t kRs485RxBufSize = 256u;

/// Default UART Tx ring-buffer size (bytes, 0 = use driver default).
inline constexpr std::size_t kRs485TxBufSize = 0u;

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Rs485Config
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief RS-485 port configuration.
 *
 * Replaces the reference `rs485_config_t` struct with explicit-width types.
 */
struct Rs485Config {
    uart_port_t uart_num{UART_NUM_1}; ///< UART peripheral number.
    gpio_num_t  tx_pin{GPIO_NUM_NC};  ///< DI (driver input = TX) pin.
    gpio_num_t  rx_pin{GPIO_NUM_NC};  ///< RO (receiver output = RX) pin.
    gpio_num_t  dir_pin{GPIO_NUM_NC}; ///< DE/RE direction control pin.
    int32_t     baud_rate{9600};      ///< Baud rate in bits-per-second (> 0).
};

// ═══════════════════════════════════════════════════════════════════════════
// § 3  IUartDriver — injectable UART abstraction
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Thin abstraction over ESP-IDF UART driver calls.
 *
 * Production code uses `EspUartDriver`.  Tests inject `MockUartDriver`.
 */
class IUartDriver {
public:
    virtual ~IUartDriver() = default;

    virtual esp_err_t param_config(uart_port_t port,
                                   const uart_config_t* cfg)              = 0;
    virtual esp_err_t set_pin(uart_port_t port,
                              int tx, int rx, int rts, int cts)           = 0;
    virtual esp_err_t driver_install(uart_port_t port,
                                     int rx_buf, int tx_buf,
                                     int queue_size,
                                     void* queue, int intr_flags)        = 0;
    virtual esp_err_t set_mode(uart_port_t port, uart_mode_t mode)        = 0;
    virtual esp_err_t driver_delete(uart_port_t port)                     = 0;

    virtual int       write_bytes(uart_port_t port,
                                  const void* src, size_t size)           = 0;
    virtual esp_err_t wait_tx_done(uart_port_t port, TickType_t ticks)    = 0;
    virtual int       read_bytes(uart_port_t port,
                                 void* buf, uint32_t length,
                                 TickType_t ticks)                        = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 4  EspUartDriver — production implementation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Production UART driver — delegates directly to ESP-IDF uart_* APIs.
 */
class EspUartDriver final : public IUartDriver {
public:
    esp_err_t param_config(uart_port_t port,
                           const uart_config_t* cfg)             override;
    esp_err_t set_pin(uart_port_t port,
                      int tx, int rx, int rts, int cts)          override;
    esp_err_t driver_install(uart_port_t port,
                             int rx_buf, int tx_buf,
                             int queue_size,
                             void* queue, int intr_flags)        override;
    esp_err_t set_mode(uart_port_t port, uart_mode_t mode)       override;
    esp_err_t driver_delete(uart_port_t port)                    override;
    int       write_bytes(uart_port_t port,
                          const void* src, size_t size)          override;
    esp_err_t wait_tx_done(uart_port_t port, TickType_t ticks)   override;
    int       read_bytes(uart_port_t port,
                         void* buf, uint32_t length,
                         TickType_t ticks)                       override;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 5  Rs485 — RAII class
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief RAII RS-485 half-duplex transceiver wrapper.
 *
 * Wraps one UART port in RS-485 half-duplex mode with automatic DE/RE
 * direction switching (handled by ESP-IDF `UART_MODE_RS485_HALF_DUPLEX`).
 *
 * Usage (production):
 * @code
 *   EspUartDriver uart;
 *   Rs485 bus{Rs485Config{.uart_num=UART_NUM_2, .tx_pin=GPIO_NUM_17,
 *                         .rx_pin=GPIO_NUM_16, .dir_pin=GPIO_NUM_5,
 *                         .baud_rate=9600}, uart};
 *   bus.init();
 *   std::array<uint8_t,7> req = {0x01,0x03,0x01,0x02,0x00,0x01,0x24,0x36};
 *   std::array<uint8_t,32> resp{};
 *   auto r = bus.send_receive(ByteView{req}, MutableByteView{resp});
 * @endcode
 *
 * Non-copyable (owns a UART resource), movable.
 */
class Rs485 {
public:
    /**
     * @param config  Port configuration (copied).
     * @param uart    Reference to a UART driver (must outlive this object).
     */
    Rs485(Rs485Config config, IUartDriver& uart) noexcept;

    /// Destructor — calls deinit() if still active (RAII).
    ~Rs485();

    Rs485(const Rs485&)            = delete;
    Rs485& operator=(const Rs485&) = delete;
    Rs485(Rs485&&)                 = default;
    Rs485& operator=(Rs485&&)      = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────

    /**
     * @brief Configure and install the UART driver, set RS-485 half-duplex mode.
     *
     * Fails with:
     *  - `InvalidParameter`  if uart_num < 0 or baud_rate <= 0.
     *  - `InvalidPinNumber`  if any GPIO pin < 0.
     *  - `Failed`            if any ESP-IDF call returns an error.
     *  - `InvalidState`      if already initialised.
     */
    Result<void> init();

    /**
     * @brief Uninstall UART driver and release hardware resources.
     *
     * Fails with `InvalidState` if not currently initialised.
     */
    Result<void> deinit();

    /// True if init() has been called successfully.
    [[nodiscard]] bool is_active() const noexcept;

    // ── I/O ───────────────────────────────────────────────────────────────

    /**
     * @brief Transmit bytes over RS-485.
     *
     * Blocks until the Tx FIFO is empty (waits for `portMAX_DELAY`).
     *
     * @param data   Read-only view over bytes to send.
     * @return `Ok` on success, `InvalidState` if not initialised,
     *         `Failed` on UART error.
     */
    Result<void> write(ByteView data);

    /**
     * @brief Receive up to `buf.size()` bytes from RS-485.
     *
     * Non-blocking if `timeout_ms` is 0; otherwise waits up to that many ms.
     *
     * @param buf         Destination buffer.
     * @param timeout_ms  Receive timeout in milliseconds.
     * @param[out] bytes_read  Number of bytes actually received.
     * @return `Ok` on success, `InvalidState` if not initialised.
     */
    Result<void> read(MutableByteView buf,
                      uint32_t timeout_ms,
                      int32_t& bytes_read);

    /**
     * @brief Send a request frame and receive a response in one call.
     *
     * Convenience method replacing the reference `rs485_context_send_receive`.
     * Transmits @p request, then reads into @p response_buf.
     *
     * @param request       Bytes to send.
     * @param response_buf  Buffer to read the response into.
     * @param timeout_ms    Receive timeout (ms). Default: 100 ms.
     * @param[out] bytes_read  Number of response bytes received.
     * @return `Ok`, `InvalidState`, `InvalidParameter` (empty buffers),
     *         or `Failed` on UART error.
     */
    Result<void> send_receive(ByteView request,
                              MutableByteView response_buf,
                              uint32_t timeout_ms,
                              int32_t& bytes_read);

    /// Return the configuration.
    [[nodiscard]] const Rs485Config& get_config() const noexcept;

private:
    Rs485Config  m_config;
    IUartDriver& m_uart;
    bool         m_active{false};
};

} // namespace fpc
