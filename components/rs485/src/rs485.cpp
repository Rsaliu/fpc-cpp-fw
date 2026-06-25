/**
 * @file rs485.cpp
 * @brief RS-485 RAII driver implementation — EspUartDriver + Rs485.
 */

#include "rs485.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr char TAG[] = "RS485";

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// EspUartDriver
// ═══════════════════════════════════════════════════════════════════════════

esp_err_t EspUartDriver::param_config(uart_port_t port, const uart_config_t* cfg)
{
    return uart_param_config(port, cfg);
}

esp_err_t EspUartDriver::set_pin(uart_port_t port, int tx, int rx, int rts, int cts)
{
    return uart_set_pin(port, tx, rx, rts, cts);
}

esp_err_t EspUartDriver::driver_install(uart_port_t port,
                                         int rx_buf, int tx_buf,
                                         int queue_size,
                                         void* queue, int intr_flags)
{
    return uart_driver_install(port, rx_buf, tx_buf,
                               queue_size,
                               static_cast<QueueHandle_t*>(queue),
                               intr_flags);
}

esp_err_t EspUartDriver::set_mode(uart_port_t port, uart_mode_t mode)
{
    return uart_set_mode(port, mode);
}

esp_err_t EspUartDriver::driver_delete(uart_port_t port)
{
    return uart_driver_delete(port);
}

int EspUartDriver::write_bytes(uart_port_t port, const void* src, size_t size)
{
    return uart_write_bytes(port, src, size);
}

esp_err_t EspUartDriver::wait_tx_done(uart_port_t port, TickType_t ticks)
{
    return uart_wait_tx_done(port, ticks);
}

int EspUartDriver::read_bytes(uart_port_t port,
                               void* buf, uint32_t length, TickType_t ticks)
{
    return uart_read_bytes(port, buf, length, ticks);
}

// ═══════════════════════════════════════════════════════════════════════════
// Rs485 — constructor / destructor
// ═══════════════════════════════════════════════════════════════════════════

Rs485::Rs485(Rs485Config config, IUartDriver& uart) noexcept
    : m_config{config}, m_uart{uart}
{}

Rs485::~Rs485()
{
    if (m_active) {
        // Best-effort — ignore errors in destructor.
        m_uart.driver_delete(m_config.uart_num);
        m_active = false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// init
// ═══════════════════════════════════════════════════════════════════════════

Result<void> Rs485::init()
{
    if (m_active) {
        return Result<void>::err(SystemError::InvalidState);
    }

    if (static_cast<int>(m_config.uart_num) < 0 || m_config.baud_rate <= 0) {
        ESP_LOGE(TAG, "init: invalid uart_num or baud_rate");
        return Result<void>::err(SystemError::InvalidParameter);
    }

    if (static_cast<int>(m_config.tx_pin)  < 0 ||
        static_cast<int>(m_config.rx_pin)  < 0 ||
        static_cast<int>(m_config.dir_pin) < 0)
    {
        ESP_LOGE(TAG, "init: invalid GPIO pin number");
        return Result<void>::err(SystemError::InvalidPinNumber);
    }

    const uart_config_t uart_cfg = {
        .baud_rate  = m_config.baud_rate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (m_uart.param_config(m_config.uart_num, &uart_cfg) != ESP_OK) {
        return Result<void>::err(SystemError::Failed);
    }

    if (m_uart.set_pin(m_config.uart_num,
                       static_cast<int>(m_config.tx_pin),
                       static_cast<int>(m_config.rx_pin),
                       static_cast<int>(m_config.dir_pin),
                       UART_PIN_NO_CHANGE) != ESP_OK)
    {
        return Result<void>::err(SystemError::Failed);
    }

    if (m_uart.driver_install(m_config.uart_num,
                               static_cast<int>(kRs485RxBufSize * 2u),
                               static_cast<int>(kRs485TxBufSize),
                               0, nullptr, 0) != ESP_OK)
    {
        return Result<void>::err(SystemError::Failed);
    }

    if (m_uart.set_mode(m_config.uart_num, UART_MODE_RS485_HALF_DUPLEX) != ESP_OK) {
        m_uart.driver_delete(m_config.uart_num);
        return Result<void>::err(SystemError::Failed);
    }

    m_active = true;
    ESP_LOGI(TAG, "[uart=%d baud=%ld] initialised",
             static_cast<int>(m_config.uart_num), m_config.baud_rate);
    return Result<void>::ok();
}

// ═══════════════════════════════════════════════════════════════════════════
// deinit
// ═══════════════════════════════════════════════════════════════════════════

Result<void> Rs485::deinit()
{
    if (!m_active) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_uart.driver_delete(m_config.uart_num);
    m_active = false;
    ESP_LOGI(TAG, "[uart=%d] deinitialised", static_cast<int>(m_config.uart_num));
    return Result<void>::ok();
}

// ═══════════════════════════════════════════════════════════════════════════
// is_active
// ═══════════════════════════════════════════════════════════════════════════

bool Rs485::is_active() const noexcept
{
    return m_active;
}

// ═══════════════════════════════════════════════════════════════════════════
// write
// ═══════════════════════════════════════════════════════════════════════════

Result<void> Rs485::write(ByteView data)
{
    if (!m_active) {
        ESP_LOGE(TAG, "write: not initialised");
        return Result<void>::err(SystemError::InvalidState);
    }

    const int written = m_uart.write_bytes(m_config.uart_num,
                                            data.data(), data.size());
    if (written < 0) {
        return Result<void>::err(SystemError::Failed);
    }

    m_uart.wait_tx_done(m_config.uart_num, portMAX_DELAY);
    ESP_LOGI(TAG, "wrote %d bytes", written);
    return Result<void>::ok();
}

// ═══════════════════════════════════════════════════════════════════════════
// read
// ═══════════════════════════════════════════════════════════════════════════

Result<void> Rs485::read(MutableByteView buf, uint32_t timeout_ms, int32_t& bytes_read)
{
    if (!m_active) {
        return Result<void>::err(SystemError::InvalidState);
    }

    const TickType_t ticks = (timeout_ms == 0u)
        ? 0
        : pdMS_TO_TICKS(timeout_ms);

    const int n = m_uart.read_bytes(m_config.uart_num,
                                     buf.data(),
                                     static_cast<uint32_t>(buf.size()),
                                     ticks);
    bytes_read = (n >= 0) ? n : 0;
    ESP_LOGI(TAG, "read %ld bytes", bytes_read);
    return Result<void>::ok();
}

// ═══════════════════════════════════════════════════════════════════════════
// send_receive
// ═══════════════════════════════════════════════════════════════════════════

Result<void> Rs485::send_receive(ByteView request,
                                  MutableByteView response_buf,
                                  uint32_t timeout_ms,
                                  int32_t& bytes_read)
{
    if (request.empty() || response_buf.empty()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    auto wr = write(request);
    if (wr.is_err()) {
        ESP_LOGE(TAG, "send_receive: write failed: %s",
                 to_string(wr.error()).data());
        return wr;
    }

    return read(response_buf, timeout_ms, bytes_read);
}

// ═══════════════════════════════════════════════════════════════════════════
// get_config
// ═══════════════════════════════════════════════════════════════════════════

const Rs485Config& Rs485::get_config() const noexcept
{
    return m_config;
}

} // namespace fpc
