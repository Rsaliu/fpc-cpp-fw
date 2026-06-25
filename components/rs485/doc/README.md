# rs485

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`rs485` provides a **RAII RS-485 / UART driver** for half-duplex communication with Modbus RTU sensors (specifically the GA1 level sensors). It abstracts the ESP-IDF `uart_*` API behind an `IUartDriver` interface so that tests can use a `MockUartDriver` without real hardware.

The `Rs485` class handles:
- Configuring the UART peripheral (baud rate, RX/TX/DIR pins)
- Installing the UART driver and ISR
- Toggling the direction pin (DE/RE) for half-duplex TX/RX switching
- Sending a request frame and receiving a response in one `send_receive()` call

---

## Public API

### `IUartDriver` — hardware abstraction

```cpp
class IUartDriver {
public:
    virtual Result<void> install(uart_port_t port, int rx_buf, int tx_buf) = 0;
    virtual Result<void> set_config(uart_port_t port, const uart_config_t& cfg) = 0;
    virtual Result<void> set_pin(uart_port_t, int tx, int rx, int rts, int cts) = 0;
    virtual Result<void> set_mode(uart_port_t port, uart_mode_t mode) = 0;
    virtual int          write(uart_port_t port, const uint8_t* data, size_t len) = 0;
    virtual int          read(uart_port_t port, uint8_t* buf, size_t len, TickType_t ticks) = 0;
    virtual Result<void> flush(uart_port_t port) = 0;
    virtual Result<void> driver_delete(uart_port_t port) = 0;
};
```

Production: `EspUartDriver` calls real `uart_*` ESP-IDF functions.  
Tests: `MockUartDriver` records calls and returns configurable data.

---

### `Rs485Config`

```cpp
struct Rs485Config {
    uart_port_t uart_num{UART_NUM_1};
    gpio_num_t  tx_pin{GPIO_NUM_NC};   // DI pin
    gpio_num_t  rx_pin{GPIO_NUM_NC};   // RO pin
    gpio_num_t  dir_pin{GPIO_NUM_NC};  // DE/RE direction control pin
    int32_t     baud_rate{9600};
};
```

---

### `Rs485`

```cpp
class Rs485 {
public:
    Rs485(Rs485Config config, IUartDriver& uart_driver);
    ~Rs485();  // calls deinit() if initialized (RAII)

    Result<void> init();
    Result<void> deinit();
    bool         is_initialized() const noexcept;

    // Send request, switch to RX, wait for response
    Result<void> send_receive(
        ByteView        request,
        MutableByteView response_buf,
        uint32_t        timeout_ms,
        int32_t&        bytes_read);
};
```

---

## Half-duplex timing

```
DIR pin HIGH  →  send request bytes
              →  wait for TX complete
DIR pin LOW   →  receive response bytes (up to timeout_ms)
              →  bytes_read = number of bytes received
```

---

## Default hardware pins (from `hardware_pins.hpp`)

| Signal | Pin |
|---|---|
| TX (DI) | GPIO 10 |
| RX (RO) | GPIO 11 |
| DIR (DE/RE) | GPIO 9 |
| UART port | UART1 |
| Baud rate | 9600 |

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `rs485_t` struct + global `rs485_context` | `Rs485` class — owns config, references `IUartDriver` |
| Raw `uint8_t*` + `size_t` buffer pairs | `ByteView` / `MutableByteView` — non-owning views |
| `error_type_t rs485_send_receive(rs485_t*, ...)` | `Result<void> Rs485::send_receive(...)` |
| No hardware abstraction layer | `IUartDriver` — mockable for unit tests |
| Manual `uart_driver_delete()` on cleanup | Destructor calls `deinit()` automatically |
