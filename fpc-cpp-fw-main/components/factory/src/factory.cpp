#include "factory.hpp"
#include "hardware_pins.hpp"

namespace fpc {

// ─── Lookup helpers ───────────────────────────────────────────────────────────

Pump* Factory::find_pump(std::vector<std::unique_ptr<Pump>>& v, int32_t id) noexcept {
    for (auto& p : v) { if (p->get_config().id == id) { return p.get(); } }
    return nullptr;
}

Tank* Factory::find_tank(std::vector<std::unique_ptr<Tank>>& v, int32_t id) noexcept {
    for (auto& t : v) { if (t->get_config().id == id) { return t.get(); } }
    return nullptr;
}

CurrentSensor* Factory::find_cs(std::vector<std::unique_ptr<CurrentSensor>>& v, int32_t id) noexcept {
    for (auto& s : v) { if (s->id() == id) { return s.get(); } }
    return nullptr;
}

LevelSensor* Factory::find_ls(std::vector<std::unique_ptr<LevelSensor>>& v, int32_t id) noexcept {
    for (auto& s : v) { if (s->id() == id) { return s.get(); } }
    return nullptr;
}

Relay* Factory::find_relay(std::vector<std::unique_ptr<Relay>>& v, int32_t id) noexcept {
    for (auto& r : v) { if (r->get_config().id == id) { return r.get(); } }
    return nullptr;
}

IPumpMonitor* Factory::find_pm(std::vector<std::unique_ptr<PumpMonitor>>& v, int32_t id) noexcept {
    for (auto& m : v) { if (m->id() == id) { return m.get(); } }
    return nullptr;
}

ITankMonitor* Factory::find_tm(std::vector<std::unique_ptr<TankMonitor>>& v, int32_t id) noexcept {
    for (auto& m : v) { if (m->id() == id) { return m.get(); } }
    return nullptr;
}

// ─── TankShape string mapper ──────────────────────────────────────────────────

static TankShape tank_shape_from_setup(std::string_view s) noexcept {
    if (s == "CYLINDRICAL" || s == "Cylinder") { return TankShape::Cylinder; }
    return TankShape::Rectangle; // default for "RECTANGULAR"
}

// ─── Main factory function ────────────────────────────────────────────────────

Result<Application> Factory::create_from_config(
        const PumpControlUnitSetupConfig& cfg) noexcept {

    Application app;

    // ── 1. Pumps ──────────────────────────────────────────────────────────────

    for (const auto& ps : cfg.pumps) {
        PumpConfig pc{
            .id                  = ps.id,
            .make                = ps.make,
            .power_hp            = ps.power_hp,
            .current_rating      = ps.current_rating,
            .min_working_current = ps.min_working_current,
        };
        app.pumps.push_back(std::make_unique<Pump>(std::move(pc)));
        if (auto r = app.pumps.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 2. Tanks ──────────────────────────────────────────────────────────────

    for (const auto& ts : cfg.tanks) {
        TankConfig tc{
            .id               = ts.id,
            .capacity_litres  = ts.capacity_litres,
            .shape            = tank_shape_from_setup(ts.shape),
            .height_cm        = ts.height_cm,
            .full_level_mm    = ts.full_level_mm,
            .low_level_mm     = ts.low_level_mm,
        };
        app.tanks.push_back(std::make_unique<Tank>(std::move(tc)));
        if (auto r = app.tanks.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 3. Relays ─────────────────────────────────────────────────────────────
    // Each Relay needs a GPIO driver that outlives it.

    for (const auto& rs : cfg.relays) {
        app.gpio_drivers.push_back(std::make_unique<EspGpioDriver>());
        RelayConfig rc{
            .id  = rs.id,
            .pin = static_cast<gpio_num_t>(rs.pin_number),
        };
        app.relays.push_back(
            std::make_unique<Relay>(rc, *app.gpio_drivers.back()));
        // NOTE: Relay::init() configures GPIO hardware.
        // In unit tests on real hardware this succeeds; on host it may not.
        if (auto r = app.relays.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 4. CurrentSensors — hardware-backed read callbacks ────────────────────
    // Use hardware-backed callback builders from current_sensor component.

    for (const auto& cs_cfg : cfg.current_sensors) {
        if (cs_cfg.make != CurrentSensorMakeType::ACS712) {
            return Result<Application>::err(SystemError::InvalidParameter);
        }

        auto cb_result = Result<ReadCallback>::err(SystemError::InvalidParameter);
        std::string sensor_make;

        if (cs_cfg.interface.type == CurrentSensorInterfaceType::InternalADC) {
            InternalAdcAcs712Config hw_cfg{};
            hw_cfg.unit = ADC_UNIT_1;
            hw_cfg.channel = static_cast<adc_channel_t>(cs_cfg.interface.channel);
            hw_cfg.atten = ADC_ATTEN_DB_12;
            hw_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
            hw_cfg.zero_voltage_mv = 2500;
            hw_cfg.sensitivity_mv_per_amp = 66.0f;
            cb_result = make_internal_adc_acs712_read_callback(hw_cfg);
            sensor_make = "ACS712 + InternalADC";
        } else if (cs_cfg.interface.type == CurrentSensorInterfaceType::ADS1115_One) {
            Ads1115Acs712Config hw_cfg{};
            hw_cfg.input_channel = cs_cfg.interface.channel;
            hw_cfg.pga_mode = board::kAds1115DefaultPga;
            hw_cfg.zero_voltage_mv = 2500;
            hw_cfg.sensitivity_mv_per_amp = 66.0f;
            cb_result = make_ads1115_acs712_read_callback(hw_cfg);
            sensor_make = "ACS712 + ADS1115";
        }

        if (cb_result.is_err()) {
            return Result<Application>::err(cb_result.error());
        }

        CurrentSensorConfig csc{
            .id      = cs_cfg.id,
            .make    = std::move(sensor_make),
            .read_cb = std::move(cb_result.value()),
        };
        app.current_sensors.push_back(
            std::make_unique<CurrentSensor>(std::move(csc)));
        if (auto r = app.current_sensors.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 5. LevelSensors — protocol + RS485 callbacks ─────────────────────────

    if (!cfg.level_sensors.empty()) {
        app.uart_drivers.push_back(std::make_unique<EspUartDriver>());

        Rs485Config bus_cfg{
            .uart_num = board::kLevelSensorRs485Uart,
            .tx_pin = board::kLevelSensorRs485TxPin,
            .rx_pin = board::kLevelSensorRs485RxPin,
            .dir_pin = board::kLevelSensorRs485DirPin,
            .baud_rate = board::kLevelSensorRs485BaudRate,
        };
        app.rs485_buses.push_back(
            std::make_unique<Rs485>(bus_cfg, *app.uart_drivers.back()));
    }

    for (const auto& ls_cfg : cfg.level_sensors) {
        Rs485* bus = app.rs485_buses.back().get();

        LevelSensorConfig lsc{
            .id            = ls_cfg.id,
            .sensor_addr   = static_cast<uint8_t>(ls_cfg.address),
            .frame_builder = [](uint8_t addr) -> protocol::gl_a01::RequestFrame {
                return protocol::gl_a01::build_read_level(addr);
            },
            .transport     = [bus](ByteView request,
                                   MutableByteView response,
                                   uint32_t timeout_ms,
                                   int32_t& n) -> Result<void> {
                if (!bus->is_active()) {
                    auto init_r = bus->init();
                    if (init_r.is_err()) {
                        return init_r;
                    }
                }
                return bus->send_receive(request, response, timeout_ms, n);
            },
            .interpreter   = [](ByteView response) -> Result<uint16_t> {
                return protocol::gl_a01::interpret_response(response);
            },
            .timeout_ms    = 100u,
        };
        app.level_sensors.push_back(
            std::make_unique<LevelSensor>(std::move(lsc)));
        if (auto r = app.level_sensors.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 6. PumpMonitors ───────────────────────────────────────────────────────

    for (const auto& pm_cfg : cfg.pump_monitors) {
        Pump* pump = find_pump(app.pumps, pm_cfg.pump_id);
        if (!pump) {
            return Result<Application>::err(SystemError::InvalidParameter);
        }

        CurrentSensor* cs = find_cs(app.current_sensors, pm_cfg.current_sensor_id);
        if (!cs) {
            return Result<Application>::err(SystemError::InvalidParameter);
        }

        // Bind the current sensor read via lambda capture
        ReadCallback read_cb = [cs]() -> Result<float> { return cs->read(); };

        PumpMonitorConfig pmc{
            .id                = pm_cfg.id,
            .pump_config       = pump->get_config(),
            .read_cb           = std::move(read_cb),
            .number_of_samples = 10,
            .analytics_cb      = current_analytics_basic_decision,
        };
        app.pump_monitors.push_back(std::make_unique<PumpMonitor>(std::move(pmc)));
        if (auto r = app.pump_monitors.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 7. TankMonitors ───────────────────────────────────────────────────────

    for (const auto& tm_cfg : cfg.tank_monitors) {
        Tank* tank = find_tank(app.tanks, tm_cfg.tank_id);
        if (!tank) {
            return Result<Application>::err(SystemError::InvalidParameter);
        }

        LevelSensor* ls = find_ls(app.level_sensors, tm_cfg.level_sensor_id);
        if (!ls) {
            return Result<Application>::err(SystemError::InvalidParameter);
        }

        LevelReadCallback level_cb = [ls]() -> Result<uint16_t> { return ls->read(); };

        TankMonitorConfig tmc{
            .id                = tm_cfg.id,
            .tank_config       = tank->get_config(),
            .level_read_cb     = std::move(level_cb),
            .number_of_samples = 10,
            .analytics_cb      = level_analytics_basic_decision,
        };
        app.tank_monitors.push_back(std::make_unique<TankMonitor>(std::move(tmc)));
        if (auto r = app.tank_monitors.back()->init(); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 8. PumpControlUnit — register all monitors ────────────────────────────

    app.control_unit = std::make_unique<PumpControlUnit>();
    if (auto r = app.control_unit->init(); r.is_err()) {
        return Result<Application>::err(r.error());
    }

    for (auto& pm : app.pump_monitors) {
        if (auto r = app.control_unit->add_pump_monitor(*pm); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }
    for (auto& tm : app.tank_monitors) {
        if (auto r = app.control_unit->add_tank_monitor(*tm); r.is_err()) {
            return Result<Application>::err(r.error());
        }
    }

    // ── 9. Subscriptions — wire relay callbacks onto monitor events ───────────

    for (const auto& sub_cfg : cfg.subscriptions) {
        for (const auto& subscriber : sub_cfg.subscribers) {
            if (subscriber.type != SubscriberType::Relay) {
                continue; // only relay subscribers supported
            }

            Relay* relay = find_relay(app.relays, subscriber.id);
            if (!relay) {
                return Result<Application>::err(SystemError::InvalidParameter);
            }

            if (sub_cfg.monitor_type == MonitorType::PumpMonitor) {
                IPumpMonitor* pm = find_pm(app.pump_monitors, sub_cfg.monitor_id);
                if (!pm) {
                    return Result<Application>::err(SystemError::InvalidParameter);
                }
                // RELAY_RESPONSE_ONE: relay ON when pump is Normal, OFF on fault
                pm->subscribe([relay](EventType event, int32_t /*slot*/) {
                    if (event == EventType::PumpNormal)       { relay->on();  }
                    if (event == EventType::PumpOvercurrent)  { relay->off(); }
                    if (event == EventType::PumpUndercurrent) { relay->off(); }
                });
            } else { // TankMonitor
                ITankMonitor* tm = find_tm(app.tank_monitors, sub_cfg.monitor_id);
                if (!tm) {
                    return Result<Application>::err(SystemError::InvalidParameter);
                }
                tm->subscribe([relay](EventType event, int32_t /*slot*/) {
                    if (event == EventType::TankFull) { relay->off(); }
                    if (event == EventType::TankLow)  { relay->on();  }
                });
            }
        }
    }

    return Result<Application>::ok(std::move(app));
}

} // namespace fpc
