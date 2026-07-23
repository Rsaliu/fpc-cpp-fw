idf_component_register(
    SRCS "src/ota_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common app_update esp_https_ota esp_http_client freertos
)
