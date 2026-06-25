/**
 * @file utils.hpp
 * @brief Miscellaneous utility free-functions.
 *
 * Provides:
 *  - swap_string  : substring replacement inside a string.
 *  - is_valid_json: JSON syntax validation via cJSON.
 *  - get_nvs_blob_size / get_nvs_blob: NVS blob helpers.
 *
 * All functions live in namespace fpc::utils.
 * Return values use fpc::Result<T> — never raw esp_err_t.
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include "common.hpp"
#include "nvs.h"           // nvs_handle_t

namespace fpc {
namespace utils {

// ─── String helpers ───────────────────────────────────────────────────────────

/**
 * @brief Replace the first occurrence of @p to_swap in @p input with
 *        @p replacement and return the resulting string.
 */
[[nodiscard]] Result<std::string> swap_string(
    std::string_view input,
    std::string_view to_swap,
    std::string_view replacement);

// ─── JSON validation ──────────────────────────────────────────────────────────

/**
 * @brief Validate that @p json_str is syntactically correct JSON.
 */
[[nodiscard]] Result<void> is_valid_json(std::string_view json_str);

// ─── NVS blob helpers ─────────────────────────────────────────────────────────

/**
 * @brief Query the byte-size of a blob stored under @p key_name in @p handle.
 */
[[nodiscard]] Result<std::size_t> get_nvs_blob_size(
    nvs_handle_t handle,
    const char*  key_name);

/**
 * @brief Read a blob from NVS and return it as a std::string.
 */
[[nodiscard]] Result<std::string> get_nvs_blob(
    nvs_handle_t handle,
    const char*  key_name,
    std::size_t  max_size);

} // namespace utils
} // namespace fpc
