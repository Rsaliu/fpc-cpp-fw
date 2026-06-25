# device_mode

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`device_mode` decides, at boot time, which operating mode the device should enter. It reads a single GPIO pin (the **mode-selection button** on GPIO 16) and routes control to one of two callbacks:

- **Button held (GPIO LOW)** → `webserver_cb()` — starts the Wi-Fi config portal.
- **Button released (GPIO HIGH)** → `main_task_cb()` — starts normal pump-control operation.

It also installs an **ANYEDGE ISR** on the button pin so that pressing the button at any time while running triggers `esp_restart()`, allowing the user to switch modes without a power cycle.

---

## Public API

### `DeviceModeConfig`

```cpp
struct DeviceModeConfig {
    gpio_num_t     button_pin{GPIO_NUM_NC}; // GPIO pin wired to the mode button
    DeviceCallback main_task_cb{};          // called when button is not pressed
    DeviceCallback webserver_cb{};          // called when button is pressed
};

using DeviceCallback = std::function<void()>;
```

---

### `DeviceMode`

```cpp
class DeviceMode final {
public:
    explicit DeviceMode(DeviceModeConfig config) noexcept;
    ~DeviceMode();  // removes ISR if init() succeeded

    // Configure GPIO input with pull-up; install ANYEDGE ISR.
    [[nodiscard]] Result<void> init();

    // Read button level and call the appropriate callback.
    [[nodiscard]] Result<void> handle_event();
};
```

---

## State machine

```
      init()
        │
        ▼
  [GPIO configured, ISR installed]
        │
   handle_event()
        │
   ┌────┴────┐
   │         │
  LOW       HIGH
(pressed) (released)
   │         │
   ▼         ▼
webserver  main_task
  _cb()      _cb()
```

At any time while running:

```
Button pressed (ANYEDGE ISR fires)
        │
        ▼
   esp_restart()   ← device reboots and re-evaluates the button state
```

---

## Usage (from `main.cpp`)

```cpp
DeviceModeConfig dm_cfg{
    .button_pin   = GPIO_NUM_16,
    .main_task_cb = main_tasks_starter,
    .webserver_cb = webserver_task_starter,
};

DeviceMode dm{dm_cfg};

if (auto r = dm.init(); r.is_err()) {
    ESP_LOGE(TAG, "DeviceMode init failed: %s", fpc::to_string(r.error()).data());
    return;
}

dm.handle_event();  // routes to whichever callback applies
```

---

## Error conditions

| Error | Cause |
|---|---|
| `SystemError::InvalidParameter` | `button_pin` is `GPIO_NUM_NC` |
| `SystemError::Failed` | `gpio_config()` or `gpio_install_isr_service()` returned an error |
| `SystemError::NullParameter` | The required callback (`main_task_cb` or `webserver_cb`) is null |

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `void (*main_cb)(void)` raw function pointer | `std::function<void()>` — can hold lambdas, captures state |
| `error_type_t` return from `init()` / `handle_event()` | `Result<void>` — typed, cannot be silently ignored |
| ISR cleanup via manual `gpio_isr_handler_remove()` calls | Handled in destructor (RAII) |

---

## See also

- [`setup_config_button`](../../setup_config_button/doc/README.md) — a companion component with identical purpose, matching the reference project's separate button handler.
