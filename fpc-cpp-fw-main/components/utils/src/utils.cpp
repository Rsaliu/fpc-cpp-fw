#include "utils.hpp"
#include <cstring>
#include "esp_log.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "UTILS";

namespace fpc {
namespace utils {

Result<std::string> swap_string(std::string_view input,
                                std::string_view to_swap,
                                std::string_view replacement)
{
    if (to_swap.empty()) {
        ESP_LOGE(TAG, "swap_string: to_swap is empty");
        return Result<std::string>::err(SystemError::InvalidParameter);
    }

    std::string src{input};
    auto pos = src.find(to_swap);
    if (pos == std::string::npos) {
        ESP_LOGE(TAG, "swap_string: substring not found");
        return Result<std::string>::err(SystemError::Failed);
    }

    src.replace(pos, to_swap.size(), replacement);
    return Result<std::string>::ok(std::move(src));
}

Result<void> is_valid_json(std::string_view json_str)
{
    if (json_str.empty()) {
        ESP_LOGE(TAG, "is_valid_json: empty string");
        return Result<void>::err(SystemError::InvalidParameter);
    }

    cJSON* json = cJSON_ParseWithLength(json_str.data(), json_str.size());
    if (json == nullptr) {
        const char* err_ptr = cJSON_GetErrorPtr();
        if (err_ptr != nullptr) {
            ESP_LOGE(TAG, "JSON parse error before: %s", err_ptr);
        } else {
            ESP_LOGE(TAG, "Unknown JSON parse error");
        }
        return Result<void>::err(SystemError::Failed);
    }

    cJSON_Delete(json);
    return Result<void>::ok();
}

Result<std::size_t> get_nvs_blob_size(nvs_handle_t handle, const char* key_name)
{
    if (handle == 0 || key_name == nullptr) {
        ESP_LOGE(TAG, "get_nvs_blob_size: null parameter");
        return Result<std::size_t>::err(SystemError::NullParameter);
    }

    std::size_t required = 0;
    esp_err_t ret = nvs_get_blob(handle, key_name, nullptr, &required);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_get_blob size failed (%s) for key '%s'",
                 esp_err_to_name(ret), key_name);
        return Result<std::size_t>::err(SystemError::Failed);
    }

    ESP_LOGI(TAG, "Blob size for key '%s': %zu bytes", key_name, required);
    return Result<std::size_t>::ok(required);
}

Result<std::string> get_nvs_blob(nvs_handle_t handle,
                                 const char*  key_name,
                                 std::size_t  max_size)
{
    if (handle == 0 || key_name == nullptr || max_size == 0) {
        ESP_LOGE(TAG, "get_nvs_blob: null / zero parameter");
        return Result<std::string>::err(SystemError::NullParameter);
    }

    std::string buf(max_size - 1, '\0');
    std::size_t read_size = max_size - 1;

    esp_err_t ret = nvs_get_blob(handle, key_name, &buf[0], &read_size);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_get_blob failed (%s) for key '%s'",
                 esp_err_to_name(ret), key_name);
        return Result<std::string>::err(SystemError::Failed);
    }

    buf.resize(read_size);
    ESP_LOGI(TAG, "Blob '%s' read — %zu bytes", key_name, read_size);
    return Result<std::string>::ok(std::move(buf));
}

} // namespace utils
} // namespace fpc
