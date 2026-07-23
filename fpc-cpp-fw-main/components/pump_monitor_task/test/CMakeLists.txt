idf_component_register(
    SRCS "test_pump_monitor_task.cpp"
    INCLUDE_DIRS "."
    REQUIRES unity pump_monitor_task
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
