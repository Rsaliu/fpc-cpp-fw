cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "src/relay_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common esp_driver_gpio
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
