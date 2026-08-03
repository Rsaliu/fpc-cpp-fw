/**
 * @file credential_store.cpp
 * @brief Implementation of CredentialStore (port of reference credential_store.c).
 */

#include "credential_store.hpp"

#include <cstring>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/platform.h"
#include "mbedtls/platform_util.h"

namespace fpc {

static const char* TAG = "CREDENTIAL_STORE";

int derive_hash(const uint8_t* pwd, std::size_t pwd_len,
                const uint8_t* salt, std::size_t salt_len,
                uint8_t out[kHashLen], uint32_t iters)
{
    return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, pwd, pwd_len,
                                         salt, salt_len, iters, kHashLen, out);
}

bool ct_equal(const uint8_t* a, const uint8_t* b, std::size_t n)
{
    uint8_t diff = 0;
    for (std::size_t i = 0; i < n; ++i) { diff |= static_cast<uint8_t>(a[i] ^ b[i]); }
    return diff == 0;
}

Result<void> CredentialStore::init()
{
    esp_err_t err = nvs_flash_init_partition(kAuthNamespace);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NVS partition %s: %s",
                 kAuthNamespace, esp_err_to_name(err));
        return Result<void>::err(SystemError::OperationFailed);
    }
    ESP_LOGI(TAG, "Credential store initialized successfully");
    return Result<void>::ok();
}

Result<void> CredentialStore::deinit()
{
    esp_err_t err = nvs_flash_deinit_partition(kAuthNamespace);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deinit NVS partition %s: %s",
                 kAuthNamespace, esp_err_to_name(err));
        return Result<void>::err(SystemError::OperationFailed);
    }
    ESP_LOGI(TAG, "Credential store deinitialized successfully");
    return Result<void>::ok();
}

Result<void> CredentialStore::set(std::string_view user, std::string_view pass)
{
    if (user.empty() || pass.empty()) {
        return Result<void>::err(SystemError::InvalidParameter);
    }
    if (user.size() >= kUsernameMax) {
        return Result<void>::err(SystemError::InvalidParameter);
    }

    AuthRecord rec{};
    rec.ver        = 1;
    rec.algo       = 1;
    rec.iterations = kPbkdf2Iters;
    std::memcpy(rec.username, user.data(), user.size());
    rec.username[user.size()] = '\0';

    esp_fill_random(rec.salt, kSaltLen);

    if (derive_hash(reinterpret_cast<const uint8_t*>(pass.data()), pass.size(),
                    rec.salt, kSaltLen, rec.hash, rec.iterations) != 0) {
        return Result<void>::err(SystemError::Failed);
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition(kAuthNamespace, kAuthKey,
                                            NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s",
                 kAuthNamespace, esp_err_to_name(err));
        return Result<void>::err(SystemError::OperationFailed);
    }
    err = nvs_set_blob(h, kAuthKey, &rec, sizeof(rec));
    if (err == ESP_OK) { err = nvs_commit(h); }
    nvs_close(h);
    mbedtls_platform_zeroize(&rec, sizeof(rec));

    return (err == ESP_OK) ? Result<void>::ok()
                           : Result<void>::err(SystemError::OperationFailed);
}

bool CredentialStore::check(std::string_view user, std::string_view pass)
{
    if (user.empty() || pass.empty()) { return false; }

    AuthRecord rec;
    std::size_t len = sizeof(rec);

    nvs_handle_t h;
    if (nvs_open_from_partition(kAuthNamespace, kAuthKey, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    esp_err_t err = nvs_get_blob(h, kAuthKey, &rec, &len);
    nvs_close(h);
    if (err != ESP_OK || len != sizeof(rec)) { return false; }

    if (std::strncmp(user.data(), rec.username, kUsernameMax) != 0) { return false; }

    uint8_t test_hash[kHashLen];
    if (derive_hash(reinterpret_cast<const uint8_t*>(pass.data()), pass.size(),
                    rec.salt, kSaltLen, test_hash, rec.iterations) != 0) {
        return false;
    }

    const bool ok = ct_equal(test_hash, rec.hash, kHashLen);
    mbedtls_platform_zeroize(test_hash, sizeof(test_hash));
    mbedtls_platform_zeroize(&rec, sizeof(rec));
    return ok;
}

Result<bool> CredentialStore::is_registered()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition(kAuthNamespace, kAuthKey,
                                            NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s",
                 kAuthNamespace, esp_err_to_name(err));
        return Result<bool>::err(SystemError::OperationFailed);
    }

    uint8_t flag = 0;
    std::size_t len = sizeof(flag);
    err = nvs_get_blob(h, kRegisteredUserKey, &flag, &len);
    nvs_close(h);

    if (err != ESP_OK) {
        // Not yet stored → treat as "not registered" (mirrors reference).
        ESP_LOGI(TAG, "Registration flag missing (%s); treating as unregistered",
                 esp_err_to_name(err));
        return Result<bool>::ok(false);
    }
    if (len == 0) {
        return Result<bool>::err(SystemError::InvalidLength);
    }
    return Result<bool>::ok(flag == kRegisteredFlag);
}

Result<void> CredentialStore::set_registered()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition(kAuthNamespace, kAuthKey,
                                            NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s",
                 kAuthNamespace, esp_err_to_name(err));
        return Result<void>::err(SystemError::OperationFailed);
    }
    const uint8_t flag = kRegisteredFlag;
    err = nvs_set_blob(h, kRegisteredUserKey, &flag, sizeof(flag));
    if (err == ESP_OK) { err = nvs_commit(h); }
    nvs_close(h);
    return (err == ESP_OK) ? Result<void>::ok()
                           : Result<void>::err(SystemError::OperationFailed);
}

Result<void> CredentialStore::clear()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open_from_partition(kAuthNamespace, kAuthKey,
                                            NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s",
                 kAuthNamespace, esp_err_to_name(err));
        return Result<void>::err(SystemError::OperationFailed);
    }
    err = nvs_erase_all(h);
    if (err == ESP_OK) { err = nvs_commit(h); }
    nvs_close(h);
    return (err == ESP_OK) ? Result<void>::ok()
                           : Result<void>::err(SystemError::OperationFailed);
}

} // namespace fpc
