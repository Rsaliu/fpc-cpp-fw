idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES common config_manager factory device_mode
                                pump_monitor_task tank_monitor_task
                                wifi_hotspot webserver_task)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)