idf_component_register(
    SRCS "src/factory.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common setup_config pump tank relay_driver level_sensor current_sensor
             pump_monitor tank_monitor pump_control_unit rs485 protocol
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
