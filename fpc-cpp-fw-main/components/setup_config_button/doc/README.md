# setup_config_button

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`setup_config_button` is a **GPIO button handler** that determines whether the device should boot into normal operation or webserver (config) mode. It is functionally identical to `device_mode` but exists as a separate component to preserve the structure of the original C reference project, which had a standalone `setup_config_button` component.

At boot, it reads the GPIO level of the mode-selection button:
- **GPIO LOW (button held)** → calls `webserver_cb`
- **GPIO HIGH (button released)** → calls `main_task_cb`

An IRAM-placed ISR is installed for the button so that pressing it at any time during operation triggers a restart.

> **Note:** `IRAM_ATTR` is applied only in the `.cpp` definition, never in this header, to comply with ESP-IDF's rules about IRAM-safe function placement.

---

## Public API

### `ButtonCallback`

```cpp
using ButtonCallback = std::function<void()>;
```

---

### `SetupConfigButtonConfig`

```cpp
struct SetupConfigButtonConfig {
    gpio_num_t     button_pin{GPIO_NUM_NC};
    ButtonCallback main_task_cb{};   // called when button not pressed at boot
    ButtonCallback webserver_cb{};   // called when button pressed at boot
};
```

---

### `SetupConfigButton`

```cpp
class SetupConfigButton final {
public:
    explicit SetupConfigButton(SetupConfigButtonConfig config) noexcept;
    ~SetupConfigButton();  // removes ISR if initialized

    Result<void> init();
    Result<void> handle_event();
    bool         is_initialized() const noexcept;
};
```

---

## Comparison with `device_mode`

| | `device_mode` | `setup_config_button` |
|---|---|---|
| Purpose | Identical | Identical |
| Why both exist | C++ rewrite component | Mirrors reference project structure |
| Restart-on-press ISR | ✓ | ✓ |
| `std::function` callbacks | ✓ | ✓ |

In practice, the main application (`main.cpp`) uses `device_mode`. `setup_config_button` is available for any code that matches the reference project's component layout.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `void (*cb)(void)` raw function pointer | `std::function<void()>` |
| `error_type_t setup_config_button_init(...)` | `Result<void> SetupConfigButton::init()` |
| `IRAM_ATTR` on ISR in header | `IRAM_ATTR` only in `.cpp` definition (ESP-IDF requirement) |
