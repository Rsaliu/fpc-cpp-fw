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
    "../components/protocol"
    "../components/device_mode"
    "../components/utils"
    "../components/rs485"
    "../components/level_sensor"
    "../components/tank_monitor"
    "../components/tank_monitor_task"
    "../components/setup_config"
    "../components/config_manager"
    "../components/setup_config_button"
    "../components/file_handler"
    "../components/serialization"
    "../components/nlohmann_json"
    "../components/event_handler_task"
    "../components/subscriber_event_task"
    "../components/mqtt_conn"
    "../components/comms_manager"
    "../components/webserver"
    "../components/webserver_task"
    "../components/ota_handler"
    "../components/wifi_hotspot"
    "../components/pump_monitor"
    "../components/current_sensor"
    "../third_party/esp-protocols/components/mdns"

)

# NOTE: current_sensor is EXCLUDED from the QEMU build: linking it pulls in
# esp_adc, whose early ADC-calibration init hangs qemu-system-xtensa (esp32s3)
# right after the eFuse calibration check. Its tests are mock-based and can
# run on real hardware. Re-evaluate on newer QEMU/IDF releases.

# ── Test components ───────────────────────────────────────────────────────
# Only list components whose tests are ready to run.
# Each entry here causes ESP-IDF to build <component>/test/ as a test app.
# NOTE: must be a CMake list (unquoted entries), not one space-separated string.
set(TEST_COMPONENTS
    common event relay_driver pump tank protocol utils
    rs485 level_sensor tank_monitor tank_monitor_task
    setup_config config_manager setup_config_button file_handler
    event_handler_task mqtt_conn comms_manager webserver webserver_task ota_handler mdns wifi_hotspot
    pump_monitor
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fpc-cpp-unity)
