/**
 * @file factory.hpp
 * @brief Wires a PumpControlUnitSetupConfig into a live component object graph.
 *
 * Given a fully parsed PumpControlUnitSetupConfig (from ConfigManager), the
 * factory creates all Pump, Tank, Relay, LevelSensor, CurrentSensor,
 * PumpMonitor and TankMonitor objects, registers them in a PumpControlUnit,
 * and wires any subscriptions (relay ↔ monitor event callbacks).
 *
 * Hardware-level callbacks are created and wired here:
 *   - CurrentSensor ReadCallback: internal ADC + ADS1115-backed ACS712
 *   - LevelSensor callbacks: GL-A01 protocol over RS485 transport
 *
 * Ownership model:
 *   Application owns every heap object via unique_ptr.
 *   PumpControlUnit holds *non-owning* pointers → monitors must outlive it.
 *   Destruction order (reverse of declaration order in Application):
 *     control_unit destroyed first, then monitors, then sensors/relays/pumps/tanks.
 */

#pragma once

#include "common.hpp"
#include "setup_config.hpp"
#include "pump.hpp"
#include "tank.hpp"
#include "relay_driver.hpp"
#include "current_sensor.hpp"
#include "level_sensor.hpp"
#include "rs485.hpp"
#include "pump_monitor.hpp"
#include "tank_monitor.hpp"
#include "pump_control_unit.hpp"
#include <memory>
#include <vector>

namespace fpc {

// ─── Application — the wired component object graph ───────────────────────────

/**
 * @brief Owns all components for one PumpControlUnit.
 *
 * Members are declared in reverse destruction order:
 *   1. control_unit — destroyed FIRST (holds non-owning monitor ptrs).
 *   2. pump_monitors, tank_monitors — destroyed after control_unit.
 *   3. current_sensors, level_sensors — sensors outlive monitors.
 *   4. pumps, tanks, relays, gpio_drivers, rs485 resources — base objects
 *      destroyed last.
 */
struct Application {
    // Destroyed last — base resources
    std::vector<std::unique_ptr<EspUartDriver>> uart_drivers;
    std::vector<std::unique_ptr<Rs485>>         rs485_buses;
    std::vector<std::unique_ptr<EspGpioDriver>> gpio_drivers;
    std::vector<std::unique_ptr<Relay>>         relays;
    std::vector<std::unique_ptr<Pump>>          pumps;
    std::vector<std::unique_ptr<Tank>>          tanks;
    std::vector<std::unique_ptr<CurrentSensor>> current_sensors;
    std::vector<std::unique_ptr<LevelSensor>>   level_sensors;

    // Destroyed after sensors — monitors reference sensors via callbacks
    std::vector<std::unique_ptr<PumpMonitor>>   pump_monitors;
    std::vector<std::unique_ptr<TankMonitor>>   tank_monitors;

    // Destroyed first — holds non-owning pointers to above monitors
    std::unique_ptr<PumpControlUnit>            control_unit;
};

// ─── Factory ──────────────────────────────────────────────────────────────────

class Factory final {
public:
    /**
     * @brief Build a fully wired Application from one PCU setup config.
     *
     * Steps performed:
     *   1. Create Pump objects from pumps[].
     *   2. Create Tank objects from tanks[].
     *   3. Create Relay objects (with EspGpioDriver) from relays[].
    *   4. Create hardware-backed CurrentSensor objects from current_sensors[].
    *   5. Create protocol/RS485-backed LevelSensor objects from level_sensors[].
     *   6. Create PumpMonitor objects, wiring PumpConfig + CurrentSensor.
     *   7. Create TankMonitor objects, wiring TankConfig + LevelSensor.
     *   8. Create and init PumpControlUnit; register all monitors.
     *   9. Wire subscriptions (relay on/off callbacks on monitor events).
     *
     * @return Fully wired Application, or:
     *   - SystemError::InvalidParameter — a required id reference is missing
     *                                     (e.g., pump_monitor.pump_id not found).
     *   - SystemError::Failed           — a component init() call failed.
     */
    [[nodiscard]] static Result<Application>
    create_from_config(const PumpControlUnitSetupConfig& cfg) noexcept;

    Factory() = delete;

private:
    // Lookup helpers — return pointer into the provided vector, or nullptr.
    static Pump*          find_pump(std::vector<std::unique_ptr<Pump>>& v, int32_t id) noexcept;
    static Tank*          find_tank(std::vector<std::unique_ptr<Tank>>& v, int32_t id) noexcept;
    static CurrentSensor* find_cs(std::vector<std::unique_ptr<CurrentSensor>>& v, int32_t id) noexcept;
    static LevelSensor*   find_ls(std::vector<std::unique_ptr<LevelSensor>>& v, int32_t id) noexcept;
    static Relay*         find_relay(std::vector<std::unique_ptr<Relay>>& v, int32_t id) noexcept;
    static IPumpMonitor*  find_pm(std::vector<std::unique_ptr<PumpMonitor>>& v, int32_t id) noexcept;
    static ITankMonitor*  find_tm(std::vector<std::unique_ptr<TankMonitor>>& v, int32_t id) noexcept;
};

} // namespace fpc
