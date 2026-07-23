idf_component_register(
    SRCS "src/tank_monitor_task.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common tank_monitor
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
