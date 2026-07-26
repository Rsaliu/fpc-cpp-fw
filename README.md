# fpc-cpp — Fluid Pump Controller (C++17)

> A complete rewrite of the original **fpc** (C) firmware into modern **C++17**, targeting the **ESP32-S3** microcontroller and built with **ESP-IDF**.

---

## Table of Contents

1. [What is this?](#1-what-is-this)
2. [fpc vs fpc-cpp — The C → C++ Conversion Story](#2-fpc-vs-fpc-cpp--the-c--c-conversion-story)
3. [Hardware Setup](#3-hardware-setup)
4. [Repository Structure](#4-repository-structure)
5. [Getting Started](#5-getting-started)
6. [Configuration](#6-configuration)
7. [Architecture Deep-Dive](#7-architecture-deep-dive)
8. [Key C++ Concepts Used](#8-key-c-concepts-used)
9. [Adding a New Component](#9-adding-a-new-component)
10. [Running the Unity Tests](#10-running-the-unity-tests)
11. [Roadmap / Known Limitations](#11-roadmap--known-limitations)
12. [Contributing](#12-contributing)
13. [License](#13-license)

---

## 1. What is this?

A **fluid pump controller** is a device that automatically monitors pumps and water tanks and reacts when something goes wrong — for example, cutting power to a pump (via a relay) when it detects that the pump is running dry (undercurrent) or is overloaded (overcurrent).

`fpc-cpp` is the firmware that runs on an **ESP32-S3** board to do exactly that:

- Reads **current sensors** (ACS712 chip, wired to an ADS1115 ADC) to watch how much current each pump draws.
- Reads **water-level sensors** (ultrasonic GA1 sensors, communicating over an RS485 bus) to track how full each tank is.
- Drives **GPIO relays** to switch pumps on or off in response to sensor events.
- Publishes telemetry over **MQTT** so you can monitor the system remotely.
- Hosts a **Wi-Fi configuration portal** so you can update the device's settings without touching the hardware.

---

## 2. fpc vs fpc-cpp — The C → C++ Conversion Story

The original project, `fpc`, was written in C. `fpc-cpp` is a ground-up rewrite using **C++17**. If you are reading code from the old project alongside this one, here is what changed and why.

> **Note:** The two projects are separate repositories. Do not mix source files between them — the APIs are not compatible.

| Old C (`fpc`) | New C++ (`fpc-cpp`) | Why it is better |
|---|---|---|
| `error_type_t` plain C enum — values pollute the global namespace and silently convert to `int` | `enum class SystemError : uint8_t` | Scoped names, no accidental integer conversions, compiler enforces exhaustive handling |
| Raw `esp_err_t` return codes that callers could silently ignore | `Result<T>` — holds either a success value **or** an error (explained in §8) | Impossible to accidentally ignore an error; the value and the error travel together |
| Raw pointers managed with `malloc` / `free` | `std::unique_ptr` (RAII — explained in §8) | Memory is freed automatically when the owning object goes out of scope; no leaks |
| `void (*callback)(void*)` raw function pointers | `std::function<Result<float>()>` | Can capture lambdas and local state; fully type-safe |
| Global C structs and file-scope variables | Classes and structs inside `namespace fpc` | Encapsulation; no name collisions with ESP-IDF internals |
| `#define CONSTANT 42` preprocessor macros | `static constexpr int kConstant = 42;` | Type-safe; visible in the debugger; cannot cause accidental token-pasting bugs |
| Bare FreeRTOS task function `void task(void*)` called via `xTaskCreate` at call sites | `PumpMonitorTask` / `TankMonitorTask` wrapper classes with `start()` returning `Result<void>` | Task lifecycle is managed by the destructor (RAII); errors surface as typed values |
| Multiple `.h` / `.c` file pairs with manually threaded state | Header-only `common.hpp` with `inline` / `constexpr` | Zero link overhead; easier to test; one include gives you everything |

---

## 3. Hardware Setup

### Target board
- **MCU:** ESP32-S3
- **Flash:** 8 MB (SPIFFS partition for configuration storage)

### Pin assignments

All fixed board-level pins live in `components/common/include/hardware_pins.hpp`.

#### ADS1115 current-sensor frontend (I²C)

| Signal | ESP32-S3 GPIO |
|---|---|
| SDA | GPIO 6 |
| SCL | GPIO 7 |
| I²C address | 0x48 |
| I²C speed | 100 kHz |

#### GA1 level sensors (RS485 / UART)

| Signal | ESP32-S3 GPIO |
|---|---|
| TX | GPIO 10 |
| RX | GPIO 11 |
| DIR (direction control) | GPIO 9 |
| Baud rate | 9600 |
| UART port | UART1 |

#### Mode selection button

| Signal | ESP32-S3 GPIO |
|---|---|
| Boot / Config button | GPIO 16 |

> **Note:** The button uses an internal pull-up. Wire it between GPIO 16 and GND. Pressing the button at boot enters **config mode**; releasing it (or not pressing it) enters **normal operation mode**.

#### Relay outputs

Relay GPIO pin numbers are **not hardcoded** — they come from `config.json` (see §6), so you can assign any available GPIO to each relay.

---

## 4. Repository Structure

```
fpc-cpp/
├── main/                   # Application entry point (main.cpp)
├── components/             # All reusable ESP-IDF components (see table below)
├── unity-app/              # Separate ESP-IDF app that runs all Unity tests on-device
├── third_party/            # Third-party libraries (esp-protocols / mDNS)
├── CMakeLists.txt          # Top-level build file
├── sdkconfig.defaults      # Default Kconfig values (C++17, exceptions, RTTI enabled)
└── sdkconfig               # Generated config (do not edit manually)
```

### Components

| Component | Kind | Purpose |
|---|---|---|
| `common` | Header-only | Shared types: `Result<T>`, `SystemError`, `Span<T>`, C++17 required |
| `setup_config` | Library | Plain data structs mirroring the JSON config schema |
| `config_manager` | Library | Parses JSON (string or SPIFFS file) → `AppSetupConfig` structs |
| `factory` | Library | Wires parsed config into a live component object graph (`Application`) |
| `device_mode` | Library | Reads boot button → routes to normal mode or config mode |
| `pump` | Library | Pump entity: config + runtime state |
| `tank` | Library | Tank entity: config + capacity calculations |
| `relay_driver` | Library | GPIO relay abstraction (open/close) |
| `current_sensor` | Library | ACS712 reads via ADS1115 or internal ADC |
| `level_sensor` | Library | GA1 ultrasonic distance sensor driver |
| `rs485` | Library | UART + RS485 half-duplex bus driver |
| `pump_monitor` | Library | State machine: Normal / Undercurrent / Overcurrent |
| `tank_monitor` | Library | Polls level sensor; tracks tank fill percentage |
| `pump_monitor_task` | Library | FreeRTOS task wrapper around `PumpMonitor` |
| `tank_monitor_task` | Library | FreeRTOS task wrapper around `TankMonitor` |
| `pump_control_unit` | Library | Orchestrates pumps, tanks and monitors for one physical PCU |
| `event` | Library | Event type definitions |
| `event_handler_task` | Library | FreeRTOS task that dispatches events to registered handlers |
| `subscriber_event_task` | Library | FreeRTOS task for subscriber-side event processing |
| `wifi_hotspot` | Library | ESP32 Wi-Fi Access Point (AP) management |
| `webserver` | Library | HTTP server built on `esp_http_server` |
| `webserver_task` | Library | FreeRTOS task wrapper around the HTTP server |
| `mqtt_conn` | Library | MQTT client |
| `comms_manager` | Library | Higher-level comms abstraction over MQTT |
| `ota_handler` | Library | Over-the-air firmware update support |
| `protocol` | Library | RS485 message framing and GA1 protocol helpers |
| `serialization` | Library | Message serialisation helpers |
| `crc` | Library | CRC calculation for RS485 message validation |
| `file_handler` | Library | SPIFFS read/write helpers |
| `utils` | Library | General utility functions |
| `setup_config_button` | Library | Debounced button read for config-mode detection |

---

## 5. Getting Started

### Prerequisites

1. **ESP-IDF v5.x** — follow the [official install guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) for your OS.
   After installation, the `idf.py` command-line tool and all required toolchains will be available.

2. **CMake ≥ 3.16** (bundled with ESP-IDF).

3. A USB cable connected to your ESP32-S3 board.

> **Note:** ESP-IDF is Espressif's official development framework. Think of it like an Arduino environment but for professional-grade embedded development — it gives you a build system, a hardware abstraction layer, FreeRTOS (a small real-time operating system), and hundreds of driver libraries.

### Clone

```bash
git clone <your-repo-url> fpc-cpp
cd fpc-cpp
```

### Set the target chip

```bash
idf.py set-target esp32s3
```

This writes the correct chip target into `sdkconfig` and regenerates the build files.

### Build

```bash
idf.py build
```

The first build takes a few minutes as ESP-IDF compiles all its internal libraries.

### Flash and monitor

```bash
idf.py flash monitor
```

This uploads the firmware over USB and opens the serial monitor so you can see log output. Press `Ctrl+]` to exit the monitor.

### All-in-one

```bash
idf.py set-target esp32s3 && idf.py build flash monitor
```

---

## 6. Configuration

The device is configured via a **JSON file** stored on the device's internal filesystem (SPIFFS — *Serial Peripheral Interface Flash File System*, a small filesystem embedded directly in the chip's flash memory).

### Config file location

```
/spiffs/config.json
```

If the file is missing, the firmware falls back to a hardcoded example (defined in `main/main.cpp`).

### Config schema

```json
{
  "site_id": "Site123",
  "device_id": "Device456",
  "pump_control_units": [
    {
      "id": 1,
      "tanks": [
        { "id": 1, "capacity_litres": 1000.0, "shape": "RECTANGULAR",
          "height_mm": 200.0 * 10 , "full_level_mm": 1800, "low_level_mm": 200 }
      ],
      "pumps": [
        { "id": 1, "make": "TestPump", "power_in_hp": 2.5,
          "current_rating": 10.0, "min_working_current": 0.5 }
      ],
      "relays": [
        { "id": 1, "pin_number": 4 }
      ],
      "current_sensors": [
        { "id": 1,
          "interface": { "type": "ADS1115_one", "channel": 1 },
          "make": "ACS712", "max_current": 20, "read_mode": "basic" }
      ],
      "level_sensors": [
        { "id": 1, "interface": "RS485", "address": 1, "protocol": "GA1" }
      ],
      "tank_monitors":  [{ "id": 1, "tank_id": 1, "level_sensor_id": 1 }],
      "pump_monitors":  [{ "id": 1, "pump_id": 1, "current_sensor_id": 1 }],
      "subscriptions": [
        {
          "monitor_type": "PUMP_MONITOR",
          "monitor_id": 1,
          "subscribers": [
            { "type": "RELAY", "id": 1, "response_type": "RELAY_RESPONSE_ONE" }
          ]
        }
      ]
    }
  ]
}
```

**Field guide:**

| Field | Description |
|---|---|
| `site_id` | Human-readable label for the installation site |
| `device_id` | Unique identifier for this device |
| `pump_control_units` | Array of independent pump-control units (most installs have one) |
| `tanks[].shape` | `"RECTANGULAR"` or `"CYLINDRICAL"` — used for volume calculations |
| `tanks[].full_level_mm` / `low_level_mm` | Sensor distance thresholds (in mm) for full/low alerts |
| `pumps[].current_rating` | Rated operating current (A) — overcurrent threshold |
| `pumps[].min_working_current` | Minimum expected current (A) — undercurrent / dry-run threshold |
| `relays[].pin_number` | GPIO pin number the relay is wired to |
| `current_sensors[].interface.type` | `"ADS1115_one"` (external ADS1115 ADC) or `"internal_adc"` |
| `level_sensors[].protocol` | `"GA1"` (only supported protocol currently) |
| `subscriptions` | Wires monitor events to actuator responses — e.g. when `PUMP_MONITOR 1` fires, activate `RELAY 1` |

### Updating the config over Wi-Fi (Config Mode)

1. **Hold the button on GPIO 16** while powering on the device (or while pressing reset).
2. The device starts a Wi-Fi Access Point called **`ESP32-WebServer`** (password: `password123`).
3. Connect your phone or laptop to that network.
4. Use any HTTP client (e.g. `curl`, Postman, or a browser) to interact with the API:

   ```bash
   # Read current config
   curl http://192.168.4.1/config

   # Push a new config
   curl -X POST http://192.168.4.1/config \
        -H "Content-Type: application/json" \
        -d @config.json

   # Health check
   curl http://192.168.4.1/health
   ```

5. After saving, **power-cycle** the device (without holding the button) to boot into normal operation with the new config.

> **Note:** Change the default AP password before deploying to production. It is set in `main/main.cpp` inside `webserver_task_fn`.

---

## 7. Architecture Deep-Dive

### Startup flow

```
app_main()
    │
    ├── DeviceMode::init()        ← configure GPIO 16 as input with pull-up
    │                               install ANYEDGE ISR (button press → esp_restart)
    │
    └── DeviceMode::handle_event()
            │
            ├── GPIO 16 LOW (button held)
            │       └── webserver_task_starter()
            │               ├── WifiHotspot::init() + on()   ← start AP
            │               └── WebserverTask::start()        ← HTTP server
            │                       GET  /health
            │                       GET  /config   → reads  /spiffs/config.json
            │                       POST /config   → writes /spiffs/config.json
            │
            └── GPIO 16 HIGH (button released)
                    └── main_tasks_starter()
                            ├── load_config()
                            │       └── ConfigManager::read_file("/spiffs/config.json")
                            │           → ConfigManager::parse()   → AppSetupConfig
                            │
                            ├── setup_system_from_config()
                            │       └── Factory::create_from_config()  → Application
                            │               (wires all unique_ptrs, callbacks, subscriptions)
                            │
                            └── setup_tasks_from_config()
                                    ├── PumpMonitorTask::start()  (100 ms poll)
                                    └── TankMonitorTask::start()  (1000 ms poll)
```

### Component ownership tree

`Application` (owned by `g_runtime` in `main.cpp`) is the root of the object graph. Destruction happens in strict reverse-declaration order to prevent dangling-pointer use-after-free:

```
Application
├── uart_drivers     (destroyed LAST — base hardware)
├── rs485_buses
├── gpio_drivers
├── relays
├── pumps
├── tanks
├── current_sensors
├── level_sensors
├── pump_monitors    (destroyed before sensors they reference)
├── tank_monitors
└── control_unit     (destroyed FIRST — holds non-owning pointers to monitors)
```

### The `Result<T>` pattern

Instead of returning raw error codes (which callers can ignore), every function that can fail returns `Result<T>`:

```cpp
// Reading a sensor either gives you a float or an error — never both, never neither.
Result<float> value = current_sensor.read();

if (value.is_ok()) {
    float amps = value.value();   // safe to use
} else {
    ESP_LOGE(TAG, "Read failed: %s", fpc::to_string(value.error()).data());
}
```

`Result<void>` is used for functions that succeed or fail but produce no value (e.g. `DeviceMode::init()`).

### Config → Object pipeline

```
JSON string / file
      │
      ▼
ConfigManager::parse()
      │  fills plain structs (no hardware, no callbacks)
      ▼
AppSetupConfig  ──► PumpControlUnitSetupConfig[]
      │
      ▼
Factory::create_from_config()
      │  allocates hardware objects, wires callbacks and subscriptions
      ▼
Application  (unique_ptr ownership graph — lives until g_runtime is destroyed)
```

---

## 8. Key C++ Concepts Used

If you are new to modern C++, here is a quick-reference glossary for patterns used throughout the codebase.

### `Result<T>` — typed error-or-value

A template class that holds **either** a success value of type `T` **or** a `SystemError`. It is like a safe, explicit version of a return code, except you cannot read the value without first checking whether it succeeded.

```cpp
Result<int> divide(int a, int b) {
    if (b == 0) return Result<int>::err(SystemError::InvalidParameter);
    return Result<int>::ok(a / b);
}
```

### `std::unique_ptr` — automatic memory management (RAII)

**RAII** stands for *Resource Acquisition Is Initialisation*. The idea: tie the lifetime of a resource (heap memory, a file handle, a hardware peripheral) to the lifetime of a C++ object. When the object is destroyed, the resource is released — automatically, even if an error occurs.

```cpp
auto pump = std::make_unique<Pump>(config);  // memory allocated
// ... use pump ...
// When pump goes out of scope (or vector is cleared), delete is called automatically.
```

### `std::function` — callable objects

`std::function<Result<float>()>` is a type that can hold *any* callable thing — a free function, a lambda, or a member-function-bound-to-an-object — as long as its signature matches `Result<float>()`. This is how sensor read callbacks are injected into monitors without the monitor needing to know anything about the hardware.

```cpp
ReadCallback cb = [&ads]() -> Result<float> {
    return ads.read_channel(1);
};
sensor.set_read_callback(cb);
```

### `enum class SystemError` — scoped enumerations

Unlike a plain C `enum`, an `enum class` keeps its values **scoped** (you write `SystemError::Busy`, not just `Busy`) and does **not** silently convert to an integer. This prevents accidental comparison with unrelated error codes.

### `constexpr` — compile-time constants

`constexpr` values are evaluated at compile time, are fully typed, and are visible in the debugger — unlike `#define` macros, which are just text substitutions with no type information.

```cpp
static constexpr const char* kConfigPath = "/spiffs/config.json";
```

### `namespace fpc` — avoiding name collisions

All project code lives inside `namespace fpc`. This means `fpc::Pump` can never be confused with any `Pump` type defined in an ESP-IDF library or third-party component.

---

## 9. Adding a New Component

Follow the existing pattern step-by-step:

1. **Create the directory structure:**
   ```
   components/my_component/
   ├── CMakeLists.txt
   ├── include/
   │   └── my_component.hpp
   ├── src/
   │   └── my_component.cpp
   └── test/
       ├── CMakeLists.txt
       └── test_my_component.cpp
   ```

2. **Write `components/my_component/CMakeLists.txt`:**
   ```cmake
   idf_component_register(
       SRCS "src/my_component.cpp"
       INCLUDE_DIRS "include"
       REQUIRES common   # add other components you depend on here
   )
   ```

3. **Declare your interface in `include/my_component.hpp`** inside `namespace fpc`. Use `Result<T>` for any fallible function.

4. **Implement in `src/my_component.cpp`**. Include your own header first, then others.

5. **Write a Unity test in `test/test_my_component.cpp`:**
   ```cpp
   #include "unity.h"
   #include "my_component.hpp"

   TEST_CASE("my_component does X", "[my_component]") {
       // ... arrange, act, assert ...
       TEST_ASSERT_TRUE(result.is_ok());
   }
   ```

6. **Register the test component in `unity-app/CMakeLists.txt`:**
   - Add `"../components/my_component"` to `EXTRA_COMPONENT_DIRS`.
   - Add `my_component` to the `TEST_COMPONENTS` string.

7. **Consume the component** from any other component's `CMakeLists.txt` by adding it to its `REQUIRES` list.

---

## 10. Running the Unity Tests

> **Note:** Unity is a unit-testing framework for C/C++. The tests run **on the actual ESP32-S3 hardware** (not on your PC), because many components test real hardware interactions. The `unity-app/` directory is a completely separate ESP-IDF project whose only job is to run all the tests.

### Build and flash the test app

```bash
cd unity-app
idf.py set-target esp32s3
idf.py build flash monitor
```

### What to expect

After flashing, the serial monitor will show each test case running and a final summary:

```
#### Running all the registered tests #####

...........

10 Tests 0 Failures 0 Ignored
OK
```

A failed test prints the file name, line number, and assertion message to help you pinpoint the problem.

### Adding tests for a new component

See step 5–6 in [§9 Adding a New Component](#9-adding-a-new-component).

---

## 11. Roadmap / Known Limitations

- **Config-mode AP credentials are hardcoded.** `ssid = "ESP32-WebServer"`, `password = "password123"` in `main.cpp`. Move these to NVS (non-volatile storage) or a provisioning flow before production use.
- **Single commit history.** The repository was created from a single "Initial Conversion" commit. Granular per-component commit history is not available for the conversion period.
- **MQTT telemetry schema** is not yet documented. See `components/comms_manager` and `components/mqtt_conn` for the current wire format.
- **OTA (over-the-air updates)** infrastructure exists in `components/ota_handler` but is not yet wired into the main application flow.
- **Cylindrical tank volume calculations** — the shape enum supports `CYLINDRICAL` but verify the formula in `components/tank` if you are using cylindrical tanks.

---

## 12. Contributing

1. Fork the repository and create a feature branch from `master`.
2. Follow the existing code style: `namespace fpc`, `Result<T>` returns, `unique_ptr` ownership, `constexpr` constants.
3. Add a Unity test for every new or changed behaviour.
4. Ensure `idf.py build` succeeds with zero warnings for both the main app and `unity-app/`.
5. Open a pull request with a clear description of what changed and why.

