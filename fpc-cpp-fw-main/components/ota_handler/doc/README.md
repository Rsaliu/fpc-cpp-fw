# ota_handler

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`ota_handler` provides a single-function interface for performing an **HTTPS OTA (Over-The-Air) firmware update**. Given an already-opened `esp_http_client_handle_t`, it downloads the new firmware binary, writes it to the OTA partition, and updates the boot descriptor — all in one call.

On success, the caller is responsible for calling `esp_restart()` to boot into the new firmware.

---

## Public API

```cpp
class OtaHandler final {
public:
    OtaHandler()  = delete;  // static-only utility class
    ~OtaHandler() = delete;

    /**
     * Download and flash OTA firmware via an open HTTP client.
     *
     * @param client  Pre-opened esp_http_client_handle_t. Must not be null.
     * @return ok()   if the partition was updated successfully.
     *         err()  on download failure, flash write error, or validation error.
     */
    [[nodiscard]] static Result<void>
    download(esp_http_client_handle_t client) noexcept;
};
```

---

## Usage

```cpp
esp_http_client_config_t http_cfg{
    .url            = "https://update.example.com/firmware.bin",
    .cert_pem       = server_root_ca_pem,
    .transport_type = HTTP_TRANSPORT_OVER_SSL,
};
auto client = esp_http_client_init(&http_cfg);

auto result = OtaHandler::download(client);
esp_http_client_cleanup(client);

if (result.is_ok()) {
    ESP_LOGI(TAG, "OTA complete — rebooting");
    esp_restart();
} else {
    ESP_LOGE(TAG, "OTA failed: %s", fpc::to_string(result.error()).data());
}
```

---

## What happens inside `download()`

1. Opens the HTTP connection and reads the response headers.
2. Calls `esp_ota_begin()` to prepare the next OTA partition.
3. Reads the firmware binary in chunks, writing each chunk with `esp_ota_write()`.
4. Calls `esp_ota_end()` to finalise and validate the image.
5. Calls `esp_ota_set_boot_partition()` to mark the new partition as the next boot target.

---

## Current status

The `ota_handler` component exists and is wired into the build system but is **not yet integrated** into the main application flow. Triggering an OTA update via MQTT command or HTTP endpoint is a planned future feature.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `error_type_t https_ota_download(esp_http_client_handle_t)` | `Result<void> OtaHandler::download(...)` — typed error return |
| Static function in a `.c` file | `static`-only class — no accidental instantiation |
