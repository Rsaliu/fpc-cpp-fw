idf_component_register(
    SRCS "src/pump_monitor.cpp" "src/current_analytics.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common event pump current_sensor
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
