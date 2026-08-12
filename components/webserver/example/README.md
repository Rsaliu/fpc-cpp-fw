# Webserver Example

[example.cpp](example.cpp) shows how to start the webserver with every route
registered, including the Swagger documentation endpoints.

This file is **not** part of the component's `SRCS`. To use it, either:

1. Copy `example_start_webserver()` into your app (e.g. `main/main.cpp`), or
2. Add the file to your own component/app `SRCS` list:

```cmake
idf_component_register(
    SRCS "main.cpp" "../components/webserver/example/example.cpp"
    ...
    REQUIRES webserver
)
```

## Prerequisites

Before calling `example_start_webserver()`, your application must have:

- Initialised NVS (`nvs_flash_init`)
- Created the default event loop and netif (`esp_event_loop_create_default`, `esp_netif_init`)
- Brought up networking (Wi-Fi STA or the hotspot component)
- Mounted the SPIFFS partition holding the frontend assets at `/www`

## Trying the API

The REST API is documented in [api/openapi.yaml](../../../api/openapi.yaml) at the
repo root. Open it in VS Code and use the *OpenAPI (Swagger) Editor* or
*Swagger Viewer* extension to preview the docs and send "Try it out" requests
against the live device (set the `deviceIp` server variable to your device's IP).
