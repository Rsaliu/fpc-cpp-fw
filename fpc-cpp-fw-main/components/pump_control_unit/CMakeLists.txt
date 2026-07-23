idf_component_register(
    SRCS "src/pump_control_unit.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common pump_monitor tank_monitor
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
