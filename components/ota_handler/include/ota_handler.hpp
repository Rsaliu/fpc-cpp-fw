/**
 * @file ota_handler.hpp
 * @brief HTTPS OTA firmware download helper.
 *
 * Single static method: OtaHandler::download(esp_http_client_handle_t).
 * On success the boot partition is updated and Result<void>::ok() is returned.
 *
 * C++17 / fpc-cpp port of https_ota_downloader.h.
 */

#pragma once

#include "common.hpp"
#include "esp_http_client.h"

namespace fpc {

class OtaHandler final {
public:
    OtaHandler()  = delete;
    ~OtaHandler() = delete;

    /**
     * @brief Download and flash OTA firmware via an open HTTP client.
     *
     * @param client  Pre-opened esp_http_client_handle_t. Must not be null.
     * @return ok() if partition updated; err() on failure.
     */
    [[nodiscard]] static Result<void> download(
        esp_http_client_handle_t client) noexcept;
};

} // namespace fpc
