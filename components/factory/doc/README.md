# factory

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `setup_config`, `pump`, `tank`, `relay_driver`, `current_sensor`, `level_sensor`, `rs485`, `pump_monitor`, `tank_monitor`, `pump_control_unit`

---

## What it does

`factory` is the **object-graph builder**. Given a fully-parsed `PumpControlUnitSetupConfig` (produced by `config_manager`), it creates every hardware object — pumps, tanks, relays, sensors, monitors — wires them together with the correct callbacks and subscriptions, and returns a single `Application` struct that owns everything via `std::unique_ptr`.

This is the C++ equivalent of the reference project's manual object-construction code scattered across `main.c`.

---

## Key types

### `Application` — owns the full component graph

```cpp
struct Application {
    // Destroyed LAST (base hardware resources)
    std::vector<std::unique_ptr<EspUartDriver>> uart_drivers;
    std::vector<std::unique_ptr<Rs485>>         rs485_buses;
    std::vector<std::unique_ptr<EspGpioDriver>> gpio_drivers;
    std::vector<std::unique_ptr<Relay>>         relays;
    std::vector<std::unique_ptr<Pump>>          pumps;
    std::vector<std::unique_ptr<Tank>>          tanks;
    std::vector<std::unique_ptr<CurrentSensor>> current_sensors;
    std::vector<std::unique_ptr<LevelSensor>>   level_sensors;

    // Destroyed AFTER sensors
    std::vector<std::unique_ptr<PumpMonitor>>   pump_monitors;
    std::vector<std::unique_ptr<TankMonitor>>   tank_monitors;

    // Destroyed FIRST (holds non-owning pointers to monitors)
    std::unique_ptr<PumpControlUnit>            control_unit;
};
```

> **Important:** Members are declared in reverse destruction order. `control_unit` is destroyed first because it holds raw (non-owning) pointers to the monitors. If it were destroyed last, those pointers would already be dangling.

---

### `Factory`

```cpp
class Factory final {
public:
    Factory()  = delete;  // static-only utility class

    [[nodiscard]] static Result<Application>
    create_from_config(const PumpControlUnitSetupConfig& cfg);
};
```

---

## What `create_from_config` wires

1. **UART drivers** and **RS485 buses** for each level sensor that uses RS485.
2. **GPIO drivers** for each relay pin.
3. **Relay** objects using the GPIO drivers.
4. **Pump** objects from pump configs.
5. **Tank** objects from tank configs.
6. **CurrentSensor** objects with ADS1115 or internal ADC `ReadCallback`.
7. **LevelSensor** objects with GA1 protocol `FrameBuilder`, `Transport`, and `ResponseInterpreter` callbacks.
8. **PumpMonitor** objects wired to pump + current sensor callbacks.
9. **TankMonitor** objects wired to tank + level sensor callbacks.
10. **PumpControlUnit** registered with all monitors.
11. **Subscriptions** — e.g. "when `PUMP_MONITOR 1` fires, call relay 1's callback".

---

## Example pipeline

```
PumpControlUnitSetupConfig  (plain data from config_manager)
            │
            ▼
     Factory::create_from_config()
            │
            ▼
        Application  (live object graph, all unique_ptrs)
            │
     ┌──────┴──────┐
     ▼             ▼
PumpMonitorTask  TankMonitorTask
  (polls pump   (polls tank
   monitors)     monitors)
```

---

## Ownership model

`Application` uses `unique_ptr` for every heap object. When `Application` goes out of scope (or is explicitly destroyed), all objects are deleted in the correct reverse-declaration order automatically — no manual cleanup required.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Manual `malloc()` + cast for each object | `std::make_unique<T>(config)` — no casts, RAII |
| Separate `init()` calls spread across `main.c` | All construction in `Factory::create_from_config()` |
| Callbacks registered via `void*` function pointer + context | Lambda captures passed as `std::function` — type-safe, no context pointer |
| Destruction order managed manually (or not at all) | Declared in reverse destruction order in `Application`; `unique_ptr` destructors fire automatically |
