idf_component_register(
    SRCS "src/wifi_hotspot.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common esp_wifi esp_netif nvs_flash
)
