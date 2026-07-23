idf_component_register(
    SRCS "src/webserver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common esp_http_server spiffs nvs_flash esp_wifi mdns
)
