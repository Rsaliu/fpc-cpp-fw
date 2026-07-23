#include "ota_handler.hpp"
#include <cstring>
#include <cerrno>
#include <cinttypes>
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "OTA_HANDLER";
#define OTA_BUF_SIZE (1024)

namespace fpc {

static void cleanup(esp_http_client_handle_t c)
{
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
}

[[noreturn]] static void fatal_loop()
{
    ESP_LOGE(TAG, "OTA fatal error — deleting task");
    vTaskDelete(nullptr);
    while (true) { ; }
}

Result<void> OtaHandler::download(esp_http_client_handle_t client) noexcept
{
    if (client == nullptr)
        return Result<void>::err(SystemError::NullParameter);

    const esp_partition_t* running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running: type=%d subtype=%d offset=0x%08" PRIx32,
             running->type, running->subtype, running->address);

    if (esp_http_client_open(client, 0) != ESP_OK) {
        cleanup(client);
        return Result<void>::err(SystemError::Failed);
    }
    esp_http_client_fetch_headers(client);

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition) { cleanup(client); return Result<void>::err(SystemError::Failed); }

    static char buf[OTA_BUF_SIZE + 1];
    esp_ota_handle_t update_handle = 0;
    bool header_checked = false;
    int  total = 0;
    esp_err_t err;

    while (true) {
        int rd = esp_http_client_read(client, buf, OTA_BUF_SIZE);
        if (rd < 0) { cleanup(client); esp_ota_abort(update_handle); fatal_loop(); }
        else if (rd > 0) {
            if (!header_checked) {
                if (rd > (int)(sizeof(esp_image_header_t)
                              + sizeof(esp_image_segment_header_t)
                              + sizeof(esp_app_desc_t))) {
                    esp_app_desc_t new_info;
                    std::memcpy(&new_info,
                        &buf[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
                        sizeof(esp_app_desc_t));
                    ESP_LOGI(TAG, "New version: %s", new_info.version);
                    header_checked = true;
                    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                    if (err != ESP_OK) { cleanup(client); esp_ota_abort(update_handle); fatal_loop(); }
                } else { cleanup(client); esp_ota_abort(update_handle); fatal_loop(); }
            }
            err = esp_ota_write(update_handle, buf, static_cast<std::size_t>(rd));
            if (err != ESP_OK) { cleanup(client); esp_ota_abort(update_handle); fatal_loop(); }
            total += rd;
        } else {
            if (errno == ECONNRESET || errno == ENOTCONN) break;
            if (esp_http_client_is_complete_data_received(client)) break;
        }
    }

    ESP_LOGI(TAG, "Total written: %d bytes", total);
    if (!esp_http_client_is_complete_data_received(client)) {
        cleanup(client); esp_ota_abort(update_handle); fatal_loop();
    }
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) { cleanup(client); fatal_loop(); }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) { cleanup(client); fatal_loop(); }

    ESP_LOGI(TAG, "OTA complete");
    return Result<void>::ok();
}

} // namespace fpc
