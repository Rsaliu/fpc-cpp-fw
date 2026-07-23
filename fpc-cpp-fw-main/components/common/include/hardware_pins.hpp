/**
 * @file hardware_pins.hpp
 * @brief Board-level fixed hardware pin/peripheral assignments.
 *
 * Keep all non-configurable sensor bus mappings here so the project has a
 * single source of truth for physical wiring.
 */

#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c.h"

namespace fpc::board {

// ─────────────────────────────────────────────────────────────────────────────
// ADS1115 (current sensor frontend)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr i2c_port_num_t kAds1115I2cPort       = I2C_NUM_1;
inline constexpr gpio_num_t     kAds1115SdaPin        = GPIO_NUM_6;
inline constexpr gpio_num_t     kAds1115SclPin        = GPIO_NUM_7;
inline constexpr uint32_t       kAds1115SclSpeedHz    = 100000U;
inline constexpr uint16_t       kAds1115DeviceAddress = 0x48U;
inline constexpr int            kAds1115DefaultPga    = 0;      // ±6.144V

// ─────────────────────────────────────────────────────────────────────────────
// RS485 bus (GA1 level sensors)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr uart_port_t    kLevelSensorRs485Uart     = UART_NUM_1;
inline constexpr gpio_num_t     kLevelSensorRs485TxPin    = GPIO_NUM_10;
inline constexpr gpio_num_t     kLevelSensorRs485RxPin    = GPIO_NUM_11;
inline constexpr gpio_num_t     kLevelSensorRs485DirPin   = GPIO_NUM_9;
inline constexpr int32_t        kLevelSensorRs485BaudRate = 9600;

} // namespace fpc::board
