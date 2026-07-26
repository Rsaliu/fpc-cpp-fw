/**
 * @file test_app_main.cpp
 * @brief Unity test runner entry point for fpc-cpp.
 *
 * This file is the `app_main` for the unity-app project.  It runs all
 * Unity test cases that were auto-registered by the TEST_CASE() macro in
 * each component's test/ directory.
 *
 * To add more components:
 *   1. Add the component dir to EXTRA_COMPONENT_DIRS in unity-app/CMakeLists.txt.
 *   2. Add the component name to TEST_COMPONENTS in unity-app/CMakeLists.txt.
 *   3. Rebuild.
 *
 * `app_main` must be declared `extern "C"` so that the ESP-IDF startup
 * sequence can call it from C linkage without name-mangling.
 */

#include <cstdio>
#include "unity.h"
#include "esp_log.h"

static constexpr char TAG[] = "TEST_RUNNER";

static void print_banner(const char* text)
{
    printf("\n#### %s #####\n\n", text);
}

extern "C" void app_main(void)
{
    print_banner("fpc-cpp: Running all registered Unity tests");
    ESP_LOGI(TAG, "Starting test suite...");

    UNITY_BEGIN();
    unity_run_all_tests();   
    UNITY_END();

    ESP_LOGI(TAG, "Test suite complete.");
}
    