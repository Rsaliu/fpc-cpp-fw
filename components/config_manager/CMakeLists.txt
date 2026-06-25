idf_component_register(
    SRCS "src/config_manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common setup_config json
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
