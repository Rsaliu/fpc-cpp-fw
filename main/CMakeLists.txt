idf_component_register(SRCS "main.cpp"
                            "app_network.cpp"
                            "experiment.cpp"

                       INCLUDE_DIRS "."
                       REQUIRES common config_manager factory device_mode
                                pump_monitor_task tank_monitor_task
                                wifi_hotspot webserver webserver_task serialization mqtt_conn
                                comms_manager ota_handler utils
                                pump current_sensor pump_monitor
                                relay_driver pump_control_unit)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)