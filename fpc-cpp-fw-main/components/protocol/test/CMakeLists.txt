cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES unity
    PRIV_REQUIRES protocol crc common
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
