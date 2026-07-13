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

Result<std::string> swap_string_all(std::string_view input,
                                    std::string_view to_swap,
                                    std::string_view replacement)
{
    if (to_swap.empty()) {
        ESP_LOGE(TAG, "swap_string_all: to_swap is empty");
        return Result<std::string>::err(SystemError::InvalidParameter);
    }

    std::string src{input};
    std::size_t pos = 0;
    while ((pos = src.find(to_swap, pos)) != std::string::npos) {
        src.replace(pos, to_swap.size(), replacement);
        pos += replacement.size();  // skip past the replacement to avoid re-matching
    }
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

Result<std::string> read_nvs_blob_from_partition(const char* partition_label,
                                                 const char* namespace_name,
                                                 const char* key_name)
{
    if (partition_label == nullptr || namespace_name == nullptr || key_name == nullptr) {
        ESP_LOGE(TAG, "read_nvs_blob_from_partition: null parameter");
        return Result<std::string>::err(SystemError::NullParameter);
    }

    // Register the partition if it hasn't been already (safe to call repeatedly).
    esp_err_t ret = nvs_flash_init_partition(partition_label);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init_partition('%s') failed: %s",
                 partition_label, esp_err_to_name(ret));
        return Result<std::string>::err(SystemError::Failed);
    }

    nvs_handle_t handle{};
    ret = nvs_open_from_partition(partition_label, namespace_name,
                                  NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open_from_partition('%s','%s') failed: %s",
                 partition_label, namespace_name, esp_err_to_name(ret));
        return Result<std::string>::err(SystemError::Failed);
    }

    auto size = get_nvs_blob_size(handle, key_name);
    if (size.is_err()) {
        nvs_close(handle);
        return Result<std::string>::err(size.error());
    }
    if (size.value() == 0) {
        ESP_LOGE(TAG, "Blob '%s' missing/empty in '%s/%s'",
                 key_name, partition_label, namespace_name);
        nvs_close(handle);
        return Result<std::string>::err(SystemError::Failed);
    }

    // +1 leaves room for a NUL terminator behind the blob bytes.
    auto blob = get_nvs_blob(handle, key_name, size.value() + 1);
    nvs_close(handle);
    return blob;
}

} // namespace utils
} // namespace fpc
