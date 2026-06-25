cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# ── Extra component directories ───────────────────────────────────────────
# Add newly implemented components here as they are completed.
set(EXTRA_COMPONENT_DIRS
    "../components/common"
    "../components/crc"
    "../components/event"
    "../components/relay_driver"
    "../components/pump"
    "../components/tank"
    "../components/rs485"
    "../components/protocol"
    "../components/level_sensor"
    "../components/current_sensor"
    "../components/pump_monitor"
    "../components/tank_monitor"
    "../components/pump_monitor_task"
    "../components/tank_monitor_task"
    "../components/pump_control_unit"
    "../components/setup_config"
    "../components/config_manager"
    "../components/factory"
    "../components/device_mode"
    "../components/utils"
    "../components/file_handler"
    "../components/serialization"
    "../components/wifi_hotspot"
    "../components/event_handler_task"
    "../components/subscriber_event_task"
    "../components/webserver"
    "../components/webserver_task"
    "../components/mqtt_conn"
    "../components/ota_handler"
    "../components/comms_manager"
    "../components/setup_config_button"
    "../third_party/esp-protocols/components/mdns"
)

# ── Test components ───────────────────────────────────────────────────────
# Only list components whose tests are ready to run.
# Each entry here causes ESP-IDF to build <component>/test/ as a test app.
set(TEST_COMPONENTS "common crc event relay_driver pump tank rs485 protocol level_sensor current_sensor pump_monitor tank_monitor pump_monitor_task tank_monitor_task pump_control_unit setup_config config_manager factory device_mode utils file_handler wifi_hotspot event_handler_task subscriber_event_task webserver webserver_task mqtt_conn ota_handler comms_manager setup_config_button")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fpc-cpp-unity)
