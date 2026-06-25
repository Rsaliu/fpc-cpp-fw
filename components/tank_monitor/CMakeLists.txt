idf_component_register(
    SRCS "src/tank_monitor.cpp" "src/level_analytics.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common event tank
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
