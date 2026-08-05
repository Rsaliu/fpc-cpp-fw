idf_component_register(
    SRCS "src/webserver.cpp"
         "src/session_manager.cpp"
         "src/credential_store.cpp"
         "src/webserver_utils.cpp"
         "src/auth.cpp"
         "src/login_handler.cpp"
         "src/register_handler.cpp"
         "src/logout_handler.cpp"
         "src/reset_handler.cpp"
         "src/config_handler.cpp"
         "src/frontend_pages_handler.cpp"
         "src/webserver_routes.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common esp_http_server spiffs nvs_flash esp_wifi mdns
             nlohmann_json mbedtls esp_timer
    PRIV_REQUIRES esp_event
)

# Build & flash the SPIFFS image from the shared frontend assets.
set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../frontend")
if(EXISTS ${WEB_SRC_DIR})
    spiffs_create_partition_image(spiffs ${WEB_SRC_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR} doesn't exist. Frontend assets are required.")
endif()
