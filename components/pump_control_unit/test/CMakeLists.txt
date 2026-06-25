idf_component_register(
    SRCS "test_pump_control_unit.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity pump_control_unit
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
