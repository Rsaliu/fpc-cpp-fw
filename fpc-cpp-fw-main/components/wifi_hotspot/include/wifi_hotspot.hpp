/**
 * @file wifi_hotspot.hpp
 * @brief RAII WiFi soft-AP wrapper — three-state machine.
 *
 * States: Uninitialized ──init()──► Initialized ──on()──► Running
 *                             ▲                               │
 *                             └──────────off()────────────────┘
 *
 * C++17 / fpc-cpp port of wifi_hotspot.h / wifi_hotspot.c.
 */

#pragma once

#include <cstdint>
#include <string>
#include "common.hpp"

namespace fpc {

enum class WifiHotspotAuthMode : uint8_t {
    Open = 0,
    WPA2 = 1,
    WPA3 = 2,
};

struct WifiHotspotConfig {
    std::string         ssid{};
    std::string         password{};
    uint8_t             channel{1};
    uint8_t             max_connections{4};
    WifiHotspotAuthMode auth_mode{WifiHotspotAuthMode::WPA2};
};

class WifiHotspot final {
public:
    explicit WifiHotspot(WifiHotspotConfig config);
    ~WifiHotspot();

    WifiHotspot(const WifiHotspot&)            = delete;
    WifiHotspot& operator=(const WifiHotspot&) = delete;

    [[nodiscard]] Result<void> init();
    [[nodiscard]] Result<void> deinit();
    [[nodiscard]] Result<void> on();
    [[nodiscard]] Result<void> off();

    [[nodiscard]] bool is_running()     const noexcept;
    [[nodiscard]] bool is_initialized() const noexcept;

private:
    enum class State : uint8_t {
        Uninitialized = 0,
        Initialized   = 1,
        Running       = 2,
    };

    WifiHotspotConfig config_;
    State             state_{State::Uninitialized};
};

} // namespace fpc
