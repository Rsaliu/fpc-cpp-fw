/**
 * @file current_sensor.hpp
 * @brief Current-sensor abstraction — ICurrentSensor interface +
 *        CurrentSensor concrete implementation with injectable callbacks.
 *
 * Architecture:
 *
 *   ICurrentSensor          ← pure interface; pump_monitor depends on this.
 *       └─ CurrentSensor    ← concrete; wired at construction time via an
 *                              injected `ReadCallback` (std::function).
 *
 * Hardware-backed callback builders are provided for:
 *   - ACS712 over internal ADC
 *   - ACS712 over ADS1115 (I2C)
 *
 * Callers can also inject custom callbacks directly:
 *
 * @code
 *   // Production: ACS712 on internal ADC
 *   auto read_fn = [&acs712]() -> Result<float> {
 *       return acs712.read_current();
 *   };
 *   CurrentSensor sensor{CurrentSensorConfig{.id=1, .make="ACS712",
 *                                            .read_cb=read_fn}};
 *
 *   // Tests: simple lambda stub
 *   CurrentSensor sensor{CurrentSensorConfig{.id=1, .make="Stub",
 *       .read_cb=[]() -> Result<float> { return Result<float>::ok(10.0f); }}};
 * @endcode
 *
 * C++17 improvements over the reference:
 *  - No `void*` context — lambda captures replace `void* callback_context`.
 *  - No `char*` make   — `std::string make`.
 *  - `Result<float>`    — replaces `error_type_t` + `float*` out-param.
 *  - Read mode is an `enum class`; overcurrent/continuous modes are now
 *    simply different `ReadCallback` implementations — no internal branching.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "common.hpp"
#include "hardware_pins.hpp"

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  ReadCallback type alias
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Callback invoked by `CurrentSensor::read()`.
 *
 * Signature: `Result<float>()`
 *
 * The callback owns all hardware context via lambda capture, so no
 * `void*` context pointer is needed here.
 *
 * Return value:
 *  - `Result<float>::ok(amps)`  — current in Amperes.
 *  - `Result<float>::err(...)`  — hardware/conversion error.
 */
using ReadCallback = std::function<Result<float>()>;

// ═══════════════════════════════════════════════════════════════════════════
// § 2  CurrentSensorConfig
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Current-sensor configuration set at construction time.
 */
struct CurrentSensorConfig {
    int32_t      id{-1};       ///< Application-level identifier (>= 0).
    std::string  make{};       ///< Sensor model name (e.g. "ACS712", "ADS1115").
    ReadCallback read_cb{};    ///< Hardware-specific read implementation.
};

// ═══════════════════════════════════════════════════════════════════════════
// § 2b  Hardware-backed callback configs
// ═══════════════════════════════════════════════════════════════════════════

struct InternalAdcAcs712Config {
    adc_unit_t      unit{ADC_UNIT_1};
    adc_channel_t   channel{ADC_CHANNEL_0};
    adc_atten_t     atten{ADC_ATTEN_DB_12};
    adc_bitwidth_t  bitwidth{ADC_BITWIDTH_DEFAULT};
    int             zero_voltage_mv{2500};
    float           sensitivity_mv_per_amp{66.0f};
};

struct Ads1115Acs712Config {
    i2c_master_dev_handle_t device_handle{nullptr}; ///< Optional external device handle.
    int                     input_channel{0};       ///< ADS1115 single-ended channel [0..3].
    int                     pga_mode{0};            ///< 0..5 => ±6.144V, ±4.096V, ... ±0.256V.
    int                     zero_voltage_mv{2500};
    float                   sensitivity_mv_per_amp{66.0f};

    // Used only when device_handle == nullptr
    i2c_port_num_t          i2c_port{board::kAds1115I2cPort};
    gpio_num_t              sda_pin{board::kAds1115SdaPin};
    gpio_num_t              scl_pin{board::kAds1115SclPin};
    uint32_t                scl_speed_hz{board::kAds1115SclSpeedHz};
    uint16_t                device_address{board::kAds1115DeviceAddress};
};

[[nodiscard]] Result<ReadCallback>
make_internal_adc_acs712_read_callback(const InternalAdcAcs712Config& cfg) noexcept;

[[nodiscard]] Result<ReadCallback>
make_ads1115_acs712_read_callback(const Ads1115Acs712Config& cfg) noexcept;

// ═══════════════════════════════════════════════════════════════════════════
// § 3  ICurrentSensor
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Pure interface for a current-sensing channel.
 *
 * `PumpMonitor` depends only on this interface; it never touches a concrete
 * sensor type, making it fully testable without hardware.
 */
class ICurrentSensor {
public:
    virtual ~ICurrentSensor() = default;

    /// Initialise the sensor channel. Must be called before `read()`.
    virtual Result<void>  init()   = 0;

    /// Release hardware resources.
    virtual Result<void>  deinit() = 0;

    /**
     * @brief Perform a synchronous current reading.
     * @return Current in Amperes, or an error code.
     */
    virtual Result<float> read()   = 0;

    /// Return the sensor's application-level ID.
    virtual int32_t       id()     const = 0;

    /// Return the sensor make/model string.
    virtual std::string_view make() const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 4  CurrentSensor — concrete implementation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Concrete current sensor — delegates `read()` to an injected callback.
 *
 * Non-copyable (represents a unique sensor channel), movable.
 */
class CurrentSensor final : public ICurrentSensor {
public:
    explicit CurrentSensor(CurrentSensorConfig config) noexcept;

    CurrentSensor(const CurrentSensor&)            = delete;
    CurrentSensor& operator=(const CurrentSensor&) = delete;
    CurrentSensor(CurrentSensor&&)                 = default;
    CurrentSensor& operator=(CurrentSensor&&)      = default;
    ~CurrentSensor() override                      = default;

    Result<void>     init()   override;
    Result<void>     deinit() override;
    Result<float>    read()   override;
    int32_t          id()     const override;
    std::string_view make()   const override;

private:
    CurrentSensorConfig m_config;
    bool                m_initialized{false};
};

} // namespace fpc
