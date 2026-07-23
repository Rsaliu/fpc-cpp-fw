idf_component_register(
    SRCS "src/device_mode.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common driver
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
