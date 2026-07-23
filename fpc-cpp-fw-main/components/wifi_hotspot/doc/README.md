# wifi_hotspot

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`wifi_hotspot` is a **RAII Wi-Fi Access Point (AP) wrapper**. It manages the ESP32's soft-AP mode — turning the device into a Wi-Fi router that other devices can connect to — using a clean three-state lifecycle.

It is used exclusively during **config mode**: when the boot button is held, the device starts the hotspot so the user can connect and push a new `config.json` via the HTTP API.

---

## State machine

```
Uninitialized ──init()──► Initialized ──on()──► Running
                    ▲                               │
                    └──────────off()────────────────┘
              deinit() returns to Uninitialized from Initialized
```

---

## Public API

### `WifiHotspotAuthMode`

```cpp
enum class WifiHotspotAuthMode : uint8_t {
    Open = 0,
    WPA2 = 1,
    WPA3 = 2,
};
```

---

### `WifiHotspotConfig`

```cpp
struct WifiHotspotConfig {
    std::string         ssid{};
    std::string         password{};
    uint8_t             channel{1};
    uint8_t             max_connections{4};
    WifiHotspotAuthMode auth_mode{WifiHotspotAuthMode::WPA2};
};
```

---

### `WifiHotspot`

```cpp
class WifiHotspot final {
public:
    explicit WifiHotspot(WifiHotspotConfig config);
    ~WifiHotspot();  // calls off() + deinit() if running (RAII)

    Result<void> init();    // configure and register the AP interface
    Result<void> deinit();  // release the AP interface
    Result<void> on();      // start broadcasting the SSID
    Result<void> off();     // stop broadcasting

    bool is_running()     const noexcept;
    bool is_initialized() const noexcept;
};
```

---

## Default configuration (from `main.cpp`)

```cpp
WifiHotspotConfig ap_cfg{};
ap_cfg.ssid            = "ESP32-WebServer";
ap_cfg.password        = "password123";
ap_cfg.channel         = 1;
ap_cfg.max_connections = 5;
ap_cfg.auth_mode       = WifiHotspotAuthMode::WPA2;
```

> **Warning:** Change the default password before deploying to production. Anyone on the same network can read and overwrite the device configuration.

---

## Typical access point address

Once `on()` succeeds, the device is reachable at `192.168.4.1` (ESP-IDF default). Connect your device to the `ESP32-WebServer` network and use:

```bash
curl http://192.168.4.1/health
curl http://192.168.4.1/config
curl -X POST http://192.168.4.1/config -d @config.json
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `wifi_init_softap()` global function with hard-coded strings | `WifiHotspot` class with `WifiHotspotConfig` struct |
| `error_type_t` returns | `Result<void>` |
| No cleanup on error paths | Destructor calls `off()` + `deinit()` automatically (RAII) |
| `WIFI_AUTH_WPA2_PSK` magic constant | `enum class WifiHotspotAuthMode` — scoped, named |
