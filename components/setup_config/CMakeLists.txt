idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES common
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS ON
)
