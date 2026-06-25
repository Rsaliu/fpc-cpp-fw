cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "src/protocol.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common crc
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
