cmake_minimum_required(VERSION 3.16)

idf_component_register(
    SRCS "src/current_sensor.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common driver esp_adc
)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
