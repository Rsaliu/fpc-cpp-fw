/**
 * @file pump_control_unit.hpp
 * @brief Registry that owns pointers to IPumpMonitor and ITankMonitor objects
 *        and drives their polling loops.
 *
 * Uses `std::unordered_map<int32_t, I*Monitor*>` for monitor registries.
 *
 * The PumpControlUnit does NOT own the monitors (non-owning pointers); the
 * monitors must outlive the PumpControlUnit.
 *
 * @code
 *   PumpMonitor pm{pm_cfg};  pm.init();
 *   TankMonitor tm{tm_cfg};  tm.init();
 *
 *   PumpControlUnit pcu;
 *   pcu.init();
 *   pcu.add_pump_monitor(pm);
 *   pcu.add_tank_monitor(tm);
 *
 *   // In a polling loop:
 *   pcu.loop_pump_monitors();   // calls check_current() on every pump monitor
 *   pcu.loop_level_monitors();  // calls check_level()   on every tank monitor
 * @endcode
 */

#pragma once

#include <cstdint>
#include <unordered_map>
#include <functional>
#include "common.hpp"
#include "pump_monitor.hpp"   // brings in IPumpMonitor
#include "tank_monitor.hpp"   // brings in ITankMonitor

namespace fpc {

class PumpControlUnit {
public:
    static constexpr int32_t kMaxMonitors = 10;

    PumpControlUnit()  = default;
    ~PumpControlUnit() = default;

    PumpControlUnit(const PumpControlUnit&)            = delete;
    PumpControlUnit& operator=(const PumpControlUnit&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    [[nodiscard]] Result<void> init();
    [[nodiscard]] Result<void> deinit();

    [[nodiscard]] bool is_initialized() const noexcept;

    // ── Pump-monitor registry ─────────────────────────────────────────────────

    /// Non-owning pointer.  Uses `monitor.id()` as the map key.
    [[nodiscard]] Result<void> add_pump_monitor(IPumpMonitor& monitor);
    [[nodiscard]] Result<void> remove_pump_monitor(int32_t id);

    // ── Tank-monitor registry ─────────────────────────────────────────────────

    [[nodiscard]] Result<void> add_tank_monitor(ITankMonitor& monitor);
    [[nodiscard]] Result<void> remove_tank_monitor(int32_t id);

    // ── Per-monitor subscriber helpers ────────────────────────────────────────

    /// Subscribe an event callback on the pump monitor identified by `pm_id`.
    /// Returns the slot index (event_id) on success.
    [[nodiscard]] Result<int32_t> add_subscriber_to_pump_monitor(
        int32_t                  pm_id,
        PumpMonitorEventCallback callback);

    [[nodiscard]] Result<void> remove_subscriber_from_pump_monitor(
        int32_t pm_id,
        int32_t event_id);

    /// Subscribe an event callback on the tank monitor identified by `tm_id`.
    /// Returns the slot index (event_id) on success.
    [[nodiscard]] Result<int32_t> add_subscriber_to_tank_monitor(
        int32_t                  tm_id,
        TankMonitorEventCallback callback);

    [[nodiscard]] Result<void> remove_subscriber_from_tank_monitor(
        int32_t tm_id,
        int32_t event_id);

    // ── Polling loops ─────────────────────────────────────────────────────────

    /// Calls `check_current()` on every registered pump monitor.
    [[nodiscard]] Result<void> loop_pump_monitors();

    /// Calls `check_level()` on every registered tank monitor.
    [[nodiscard]] Result<void> loop_level_monitors();

private:
    std::unordered_map<int32_t, std::reference_wrapper<IPumpMonitor>> pump_monitors_;
    std::unordered_map<int32_t, std::reference_wrapper<ITankMonitor>> tank_monitors_;
    bool initialized_{false};
};

}  // namespace fpc
