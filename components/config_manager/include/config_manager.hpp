/**
 * @file config_manager.hpp
 * @brief JSON → AppSetupConfig parser using cJSON (ESP-IDF built-in).
 *
 * Usage:
 *   // From a JSON string:
 *   auto result = ConfigManager::parse(json_str);
 *   if (result.is_ok()) {
 *       const AppSetupConfig& cfg = result.value();
 *       // Pass to Factory::create_from_config()
 *   }
 *
 *   // From a file path on SPIFFS / LittleFS:
 *   auto str_result = ConfigManager::read_file("/spiffs/config.json");
 *   if (str_result.is_ok()) {
 *       auto cfg_result = ConfigManager::parse(str_result.value());
 *   }
 */

#pragma once

#include "common.hpp"
#include "setup_config.hpp"
#include <string>
#include <string_view>

namespace fpc {

class ConfigManager final {
public:
    /**
     * @brief Parse JSON string into AppSetupConfig.
     * @return Populated AppSetupConfig, or SystemError::InvalidParameter on bad JSON.
     */
    [[nodiscard]] static Result<AppSetupConfig>
    parse(std::string_view json_str) noexcept;

    /**
     * @brief Read a file from the filesystem into a string.
     * @return File contents, or SystemError::Failed if the file cannot be opened.
     */
    [[nodiscard]] static Result<std::string>
    read_file(const char* path) noexcept;

    ConfigManager() = delete;
};

} // namespace fpc
