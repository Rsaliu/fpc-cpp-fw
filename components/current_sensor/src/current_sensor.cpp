/**
 * @file current_sensor.cpp
 * @brief CurrentSensor concrete implementation.
 */

#include "current_sensor.hpp"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <array>
#include <memory>

static constexpr char TAG[] = "CURRENT_SENSOR";

namespace fpc {

namespace {

constexpr std::array<float, 6> kAds1115PgaVoltages = {
    6.144f, 4.096f, 2.048f, 1.024f, 0.512f, 0.256f
};

constexpr float kAds1115FullRange = 32768.0f;

class InternalAdcAcs712Reader final {
public:
    explicit InternalAdcAcs712Reader(InternalAdcAcs712Config cfg) noexcept
        : cfg_{cfg} {}

    ~InternalAdcAcs712Reader() {
        if (cali_initialized_) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
            (void)adc_cali_delete_scheme_curve_fitting(cali_handle_);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
            (void)adc_cali_delete_scheme_line_fitting(cali_handle_);
#endif
            cali_initialized_ = false;
        }
        if (initialized_ && adc_handle_ != nullptr) {
            (void)adc_oneshot_del_unit(adc_handle_);
            adc_handle_ = nullptr;
            initialized_ = false;
        }
    }

    Result<float> read_current() {
        if (!initialized_) {
            if (auto r = init(); r.is_err()) {
                return Result<float>::err(r.error());
            }
        }

        int raw = 0;
        if (adc_oneshot_read(adc_handle_, cfg_.channel, &raw) != ESP_OK) {
            return Result<float>::err(SystemError::OperationFailed);
        }

        int mv = raw;
        if (cali_initialized_) {
            if (adc_cali_raw_to_voltage(cali_handle_, raw, &mv) != ESP_OK) {
                return Result<float>::err(SystemError::OperationFailed);
            }
        } else {
            constexpr float max_mv = 3300.0f;
            constexpr float max_raw = 4095.0f;
            mv = static_cast<int>((static_cast<float>(raw) / max_raw) * max_mv);
        }

        const float current =
            (static_cast<float>(mv - cfg_.zero_voltage_mv)) / cfg_.sensitivity_mv_per_amp;
        return Result<float>::ok(current);
    }

private:
    Result<void> init() {
        adc_oneshot_unit_init_cfg_t init_cfg{};
        init_cfg.unit_id = cfg_.unit;

        if (adc_oneshot_new_unit(&init_cfg, &adc_handle_) != ESP_OK) {
            return Result<void>::err(SystemError::OperationFailed);
        }

        adc_oneshot_chan_cfg_t chan_cfg{};
        chan_cfg.atten = cfg_.atten;
        chan_cfg.bitwidth = cfg_.bitwidth;
        if (adc_oneshot_config_channel(adc_handle_, cfg_.channel, &chan_cfg) != ESP_OK) {
            (void)adc_oneshot_del_unit(adc_handle_);
            adc_handle_ = nullptr;
            return Result<void>::err(SystemError::OperationFailed);
        }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t cal_cfg{};
        cal_cfg.unit_id = cfg_.unit;
        cal_cfg.chan = cfg_.channel;
        cal_cfg.atten = cfg_.atten;
        cal_cfg.bitwidth = cfg_.bitwidth;
        if (adc_cali_create_scheme_curve_fitting(&cal_cfg, &cali_handle_) == ESP_OK) {
            cali_initialized_ = true;
        }
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_line_fitting_config_t cal_cfg{};
        cal_cfg.unit_id = cfg_.unit;
        cal_cfg.atten = cfg_.atten;
        cal_cfg.bitwidth = cfg_.bitwidth;
        if (adc_cali_create_scheme_line_fitting(&cal_cfg, &cali_handle_) == ESP_OK) {
            cali_initialized_ = true;
        }
#endif

        initialized_ = true;
        return Result<void>::ok();
    }

    InternalAdcAcs712Config    cfg_;
    adc_oneshot_unit_handle_t  adc_handle_{nullptr};
    adc_cali_handle_t          cali_handle_{nullptr};
    bool                       initialized_{false};
    bool                       cali_initialized_{false};
};

class Ads1115Acs712Reader final {
public:
    explicit Ads1115Acs712Reader(Ads1115Acs712Config cfg) noexcept
        : cfg_{cfg} {}

    ~Ads1115Acs712Reader() {
        if (owns_device_ && bus_handle_ != nullptr) {
            (void)i2c_del_master_bus(bus_handle_);
            bus_handle_ = nullptr;
            dev_handle_ = nullptr;
        }
    }

    Result<float> read_current() {
        if (!initialized_) {
            if (auto r = init(); r.is_err()) {
                return Result<float>::err(r.error());
            }
        }

        auto mv_res = read_one_shot_mv();
        if (mv_res.is_err()) {
            return Result<float>::err(mv_res.error());
        }

        const int mv = mv_res.value();
        const float current =
            (static_cast<float>(mv - cfg_.zero_voltage_mv)) / cfg_.sensitivity_mv_per_amp;
        return Result<float>::ok(current);
    }

private:
    Result<void> init() {
        if (cfg_.input_channel < 0 || cfg_.input_channel > 3) {
            return Result<void>::err(SystemError::InvalidParameter);
        }
        if (cfg_.pga_mode < 0 || cfg_.pga_mode > 5) {
            return Result<void>::err(SystemError::InvalidParameter);
        }

        if (cfg_.device_handle != nullptr) {
            dev_handle_ = cfg_.device_handle;
            initialized_ = true;
            return Result<void>::ok();
        }

        i2c_master_bus_config_t bus_cfg{};
        bus_cfg.i2c_port = cfg_.i2c_port;
        bus_cfg.sda_io_num = cfg_.sda_pin;
        bus_cfg.scl_io_num = cfg_.scl_pin;
        bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_cfg.glitch_ignore_cnt = 0;
        bus_cfg.flags.enable_internal_pullup = 1;

        if (i2c_new_master_bus(&bus_cfg, &bus_handle_) != ESP_OK) {
            return Result<void>::err(SystemError::OperationFailed);
        }

        i2c_device_config_t dev_cfg{};
        dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_cfg.device_address = cfg_.device_address;
        dev_cfg.scl_speed_hz = cfg_.scl_speed_hz;

        if (i2c_master_bus_add_device(bus_handle_, &dev_cfg, &dev_handle_) != ESP_OK) {
            (void)i2c_del_master_bus(bus_handle_);
            bus_handle_ = nullptr;
            return Result<void>::err(SystemError::OperationFailed);
        }

        owns_device_ = true;
        initialized_ = true;
        return Result<void>::ok();
    }

    Result<int> read_one_shot_mv() {
        uint16_t config = 0;
        config |= (1u << 15);                                      // OS: start single conversion
        config |= (static_cast<uint16_t>(cfg_.input_channel + 4) & 0x7u) << 12; // single-ended mux
        config |= (static_cast<uint16_t>(cfg_.pga_mode) & 0x7u) << 9;            // PGA
        config |= (1u << 8);                                       // MODE: single-shot
        config |= (0x7u << 5);                                     // DR: 860 SPS
        config |= 0x3u;                                            // COMP_QUE disable comparator

        uint8_t cfg_bytes[2] = {
            static_cast<uint8_t>(config >> 8),
            static_cast<uint8_t>(config & 0xFF),
        };

        if (write_register(0x01, cfg_bytes, sizeof(cfg_bytes)).is_err()) {
            return Result<int>::err(SystemError::OperationFailed);
        }

        uint8_t data[2] = {0, 0};
        if (read_register(0x00, data, sizeof(data)).is_err()) {
            return Result<int>::err(SystemError::OperationFailed);
        }

        const int16_t raw = static_cast<int16_t>((data[0] << 8) | data[1]);
        const float full_scale = kAds1115PgaVoltages[static_cast<size_t>(cfg_.pga_mode)];
        const float volts = (static_cast<float>(raw) / kAds1115FullRange) * full_scale;
        const int mv = static_cast<int>(volts * 1000.0f);
        return Result<int>::ok(mv);
    }

    Result<void> write_register(uint8_t reg, const uint8_t* payload, size_t size) {
        std::array<uint8_t, 3> tx{};
        if (size > (tx.size() - 1)) {
            return Result<void>::err(SystemError::InvalidLength);
        }
        tx[0] = reg;
        for (size_t i = 0; i < size; ++i) {
            tx[i + 1] = payload[i];
        }
        if (i2c_master_transmit(dev_handle_, tx.data(), size + 1, 1000) != ESP_OK) {
            return Result<void>::err(SystemError::OperationFailed);
        }
        return Result<void>::ok();
    }

    Result<void> read_register(uint8_t reg, uint8_t* out, size_t size) {
        if (i2c_master_transmit_receive(dev_handle_, &reg, 1, out, size, 1000) != ESP_OK) {
            return Result<void>::err(SystemError::OperationFailed);
        }
        return Result<void>::ok();
    }

    Ads1115Acs712Config        cfg_;
    i2c_master_bus_handle_t    bus_handle_{nullptr};
    i2c_master_dev_handle_t    dev_handle_{nullptr};
    bool                       owns_device_{false};
    bool                       initialized_{false};
};

} // namespace

CurrentSensor::CurrentSensor(CurrentSensorConfig config) noexcept
    : m_config{std::move(config)}
{}

Result<void> CurrentSensor::init()
{
    if (m_config.id < 0 || m_config.make.empty()) {
        ESP_LOGE(TAG, "[id=%ld] init: invalid config", m_config.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (!m_config.read_cb) {
        ESP_LOGE(TAG, "[id=%ld] init: read_cb is null", m_config.id);
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_initialized = true;
    ESP_LOGI(TAG, "[id=%ld make=%s] initialised", m_config.id, m_config.make.c_str());
    return Result<void>::ok();
}

Result<void> CurrentSensor::deinit()
{
    if (!m_initialized) {
        return Result<void>::err(SystemError::InvalidState);
    }
    m_initialized = false;
    ESP_LOGI(TAG, "[id=%ld] deinitialised", m_config.id);
    return Result<void>::ok();
}

Result<float> CurrentSensor::read()
{
    if (!m_initialized) {
        return Result<float>::err(SystemError::InvalidState);
    }
    auto result = m_config.read_cb();
    if (result.is_ok()) {
        ESP_LOGI(TAG, "[id=%ld] current=%.3f A", m_config.id, result.value());
    } else {
        ESP_LOGE(TAG, "[id=%ld] read error: %s",
                 m_config.id, to_string(result.error()).data());
    }
    return result;
}

int32_t CurrentSensor::id() const
{
    return m_config.id;
}

std::string_view CurrentSensor::make() const
{
    return m_config.make;
}

Result<ReadCallback>
make_internal_adc_acs712_read_callback(const InternalAdcAcs712Config& cfg) noexcept
{
    auto reader = std::make_shared<InternalAdcAcs712Reader>(cfg);
    ReadCallback cb = [reader]() -> Result<float> {
        return reader->read_current();
    };
    return Result<ReadCallback>::ok(std::move(cb));
}

Result<ReadCallback>
make_ads1115_acs712_read_callback(const Ads1115Acs712Config& cfg) noexcept
{
    auto reader = std::make_shared<Ads1115Acs712Reader>(cfg);
    ReadCallback cb = [reader]() -> Result<float> {
        return reader->read_current();
    };
    return Result<ReadCallback>::ok(std::move(cb));
}

} // namespace fpc
