# pump_control_unit

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `pump_monitor`, `tank_monitor`

---

## What it does

`pump_control_unit` is the **top-level orchestrator** for a single physical pump-control unit. It holds **non-owning pointers** to all registered `IPumpMonitor` and `ITankMonitor` instances and provides methods to poll them in a loop.

> **Ownership note:** `PumpControlUnit` does **not** own the monitors. Ownership lives in `Application` (in `factory`). `PumpControlUnit` must be destroyed before the monitors it references.

---

## Public API

```cpp
class PumpControlUnit {
public:
    static constexpr int32_t kMaxMonitors = 10;

    Result<void> init();
    Result<void> deinit();
    bool         is_initialized() const noexcept;

    // ── Pump-monitor registry ───────────────────────────────────────────
    Result<void> add_pump_monitor(IPumpMonitor& monitor);
    Result<void> remove_pump_monitor(int32_t id);

    // ── Tank-monitor registry ───────────────────────────────────────────
    Result<void> add_tank_monitor(ITankMonitor& monitor);
    Result<void> remove_tank_monitor(int32_t id);

    // ── Subscriber helpers ──────────────────────────────────────────────
    // Subscribe an event callback on a specific pump monitor.
    // Returns the subscriber slot id on success.
    Result<int32_t> add_subscriber_to_pump_monitor(
        int32_t pm_id,
        PumpMonitorEventCallback callback);

    // ── Polling (called from PumpMonitorTask / TankMonitorTask) ─────────
    Result<void> loop_pump_monitors();   // calls check_current() on each
    Result<void> loop_level_monitors();  // calls check_level()   on each
};
```

---

## Monitor registries

Internally both registries use `std::unordered_map<int32_t, I*Monitor*>` for O(1) average lookup by monitor ID, up to `kMaxMonitors = 10`.

```
PumpControlUnit
├── pump_monitors_: { 1 → &pm1, 2 → &pm2, ... }
└── tank_monitors_: { 1 → &tm1, 2 → &tm2, ... }
```

---

## Polling loop (called from task wrappers)

```cpp
// PumpMonitorTask calls this every check_interval_ms:
pcu.loop_pump_monitors();
// → calls pm1.check_current(), pm2.check_current(), ...

// TankMonitorTask calls this every check_interval_ms:
pcu.loop_level_monitors();
// → calls tm1.check_level(), tm2.check_level(), ...
```

---

## Subscriptions

A *subscription* wires a monitor's events to an actuator response. Example: "when `PUMP_MONITOR 1` fires a `PumpOvercurrent` event, call `relay->off()`".

```cpp
pcu.add_subscriber_to_pump_monitor(
    /*pm_id=*/ 1,
    [&relay](EventType evt, int32_t /*slot*/) {
        if (evt == EventType::PumpOvercurrent) relay.off();
        if (evt == EventType::PumpNormal)      relay.on();
    });
```

These subscriptions are set up by `Factory` using the `subscriptions` array in `config.json`.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Array of `pump_monitor_t*` with linear search | `std::unordered_map<int32_t, IPumpMonitor*>` — O(1) lookup |
| `void*` subscriber context | `std::function<void(EventType, int32_t)>` — type-safe callback |
| Manual loop with null checks | `loop_pump_monitors()` iterates the map, returns `Result<void>` |
