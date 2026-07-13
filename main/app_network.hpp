/**
 * @file app_network.hpp
 * @brief Wi-Fi station + MQTTS client + HTTPS-OTA startup.
 *
 * Port of fpc/main/init.c networking flow: connects the Wi-Fi station, loads
 * TLS certificates from the `secrets` NVS partition, derives the device ID from
 * the eFUSE MAC, builds the OTA topic set, connects over mqtts://, subscribes to
 * the job/rollback topics, and dispatches inbound messages via RouteManager.
 */

#pragma once

#include "common.hpp"

namespace fpc::app {

/**
 * @brief Blocking connect of the Wi-Fi station using the compiled-in credentials.
 * @return ok() once an IP is obtained; err() on timeout/failure.
 */
[[nodiscard]] Result<void> init_wifi_station();

/**
 * @brief Run an HTTPS OTA using the `https_cert` blob from the `secrets` NVS
 *        partition to verify the firmware host.
 * @param firmware_url  Fully-qualified https:// URL of the firmware image.
 * @return ok() if the OTA image was written (caller should esp_restart()).
 */
[[nodiscard]] Result<void> run_https_ota(const char* firmware_url);

/**
 * @brief FreeRTOS task entry point: Wi-Fi + MQTTS connect + subscribe + route
 *        dispatch loop. Never returns.
 */
void mqtt_client_task(void* params);

} // namespace fpc::app
