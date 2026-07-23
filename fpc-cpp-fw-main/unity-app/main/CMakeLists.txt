idf_component_register(
    SRCS "test_app_main.cpp"
    INCLUDE_DIRS "."
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
