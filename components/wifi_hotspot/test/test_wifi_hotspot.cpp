#include "unity.h"
#include "wifi_hotspot.hpp"

static fpc::WifiHotspotConfig make_valid_config()
{
    fpc::WifiHotspotConfig cfg;
    cfg.ssid            = "TestAP";
    cfg.password        = "Password123";
    cfg.channel         = 6;
    cfg.max_connections = 4;
    cfg.auth_mode       = fpc::WifiHotspotAuthMode::WPA2;
    return cfg;
}

TEST_CASE("WifiHotspot: default constructed is not initialized", "[wifi_hotspot]")
{
    fpc::WifiHotspot hs{make_valid_config()};
    TEST_ASSERT_FALSE(hs.is_initialized());
    TEST_ASSERT_FALSE(hs.is_running());
}

TEST_CASE("WifiHotspot: on() before init returns InvalidState", "[wifi_hotspot]")
{
    fpc::WifiHotspot hs{make_valid_config()};
    auto r = hs.on();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
}

TEST_CASE("WifiHotspot: off() before init returns InvalidState", "[wifi_hotspot]")
{
    fpc::WifiHotspot hs{make_valid_config()};
    auto r = hs.off();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
}

TEST_CASE("WifiHotspot: deinit() before init returns InvalidState", "[wifi_hotspot]")
{
    fpc::WifiHotspot hs{make_valid_config()};
    auto r = hs.deinit();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidState, (int)r.error());
}

TEST_CASE("WifiHotspot: empty SSID returns InvalidParameter on init", "[wifi_hotspot]")
{
    fpc::WifiHotspotConfig cfg = make_valid_config();
    cfg.ssid = "";
    fpc::WifiHotspot hs{cfg};
    TEST_ASSERT_TRUE(hs.init().is_err());
}

TEST_CASE("WifiHotspot: SSID too long returns InvalidParameter on init", "[wifi_hotspot]")
{
    fpc::WifiHotspotConfig cfg = make_valid_config();
    cfg.ssid = std::string(32, 'A');
    fpc::WifiHotspot hs{cfg};
    auto r = hs.init();
    TEST_ASSERT_TRUE(r.is_err());
    TEST_ASSERT_EQUAL_INT((int)fpc::SystemError::InvalidParameter, (int)r.error());
}

TEST_CASE("WifiHotspot: WPA2 with short password returns InvalidParameter on init", "[wifi_hotspot]")
{
    fpc::WifiHotspotConfig cfg = make_valid_config();
    cfg.password = "short";
    fpc::WifiHotspot hs{cfg};
    TEST_ASSERT_TRUE(hs.init().is_err());
}

TEST_CASE("WifiHotspot: invalid channel returns InvalidParameter on init", "[wifi_hotspot]")
{
    fpc::WifiHotspotConfig cfg = make_valid_config();
    cfg.channel = 14;
    fpc::WifiHotspot hs{cfg};
    TEST_ASSERT_TRUE(hs.init().is_err());
}

TEST_CASE("WifiHotspot: invalid max_connections returns InvalidParameter on init", "[wifi_hotspot]")
{
    fpc::WifiHotspotConfig cfg = make_valid_config();
    cfg.max_connections = 11;
    fpc::WifiHotspot hs{cfg};
    TEST_ASSERT_TRUE(hs.init().is_err());
}
