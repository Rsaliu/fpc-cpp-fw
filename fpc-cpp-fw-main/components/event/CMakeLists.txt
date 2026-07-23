cmake_minimum_required(VERSION 3.16)

idf_component_register(
    # Header-only component — no source files.
    INCLUDE_DIRS "include"
    REQUIRES common
)

# Propagate C++17 + GNU extensions to every consumer.
target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_17)
target_compile_options(${COMPONENT_LIB} INTERFACE -std=gnu++17)
