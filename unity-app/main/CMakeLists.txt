idf_component_register(
    SRCS "test_app_main.cpp"
    INCLUDE_DIRS "."
)

# When the FPC_SKIP_HW_TESTS env var is set (e.g. in CI running under QEMU),
# compile the runner so it skips tests tagged [hw]. Locally, build without
# the flag and all tests run.
if(DEFINED ENV{FPC_SKIP_HW_TESTS} AND NOT "$ENV{FPC_SKIP_HW_TESTS}" STREQUAL "0")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FPC_SKIP_HW_TESTS=1)
    message(STATUS "unity-app: [hw]-tagged tests will be SKIPPED (FPC_SKIP_HW_TESTS set)")
endif()

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
