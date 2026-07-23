# webserver

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`webserver` provides a **RAII HTTP server wrapper** around ESP-IDF's `esp_http_server`. It defines a pure `IWebServer` interface (so tests can mock the server) and a concrete `Webserver` class that manages the full server lifecycle: Uninitialized → Initialized → Running → Stopped.

`webserver_task` owns a `Webserver` instance and manages it inside a FreeRTOS task. Routes are registered via an injected `WebserverSetupFn` callback.

---

## State machine

```
Uninitialized ──init()──► Initialized ──start()──► Running ──stop()──► Stopped
                                 ▲                                         │
                                 └──────────────deinit()───────────────────┘
```

---

## Key constants

```cpp
static constexpr std::size_t kWebserverMaxPathLen = 256U;  // max path length
static constexpr std::size_t kScratchBufSize      = 10240U; // HTTP handler body buffer
```

---

## Public API

### `WebserverConfig`

```cpp
struct WebserverConfig {
    int         port{80};
    std::string document_root{};
    int         max_connections{7};
    std::string mdns_instance{};        // mDNS service instance name
    std::string mdns_hostname{};        // mDNS hostname (e.g. "fpc-webserver")
    std::string base_path{};            // filesystem base path (e.g. "/spiffs")
    std::string web_mount_point{};
    std::string web_partition_label{};
    std::string config_file_path{};     // relative path to config.json
};
```

### `WebserverContext`

Passed to HTTP handlers as `req->user_ctx`:

```cpp
struct WebserverContext {
    char base_path[kWebserverMaxPathLen + 1];
    char scratch[kScratchBufSize];          // reusable scratch buffer for request bodies
    char config_file_path[kWebserverMaxPathLen + 1];
};
```

### `IWebServer` — pure interface

```cpp
class IWebServer {
public:
    virtual Result<void> init()   = 0;
    virtual Result<void> start()  = 0;
    virtual Result<void> stop()   = 0;
    virtual Result<void> deinit() = 0;

    virtual Result<void> add_route(httpd_uri_t* uri)                          = 0;
    virtual Result<void> remove_route(const char* uri, httpd_method_t method) = 0;

    virtual httpd_handle_t    get_handle()  const noexcept = 0;
    virtual WebserverContext* get_context() const noexcept = 0;
};
```

### `Webserver` — concrete implementation

```cpp
class Webserver final : public IWebServer {
public:
    explicit Webserver(WebserverConfig config);
    ~Webserver() override;  // calls stop() + deinit() if still running
    // ...implements all IWebServer methods
};
```

---

## Routes registered by the main app

The main application registers these routes via a `WebserverSetupFn` in `main.cpp`:

| Method | Path | Handler |
|---|---|---|
| `GET` | `/health` | Returns `{"status":"ok"}` |
| `GET` | `/config` | Reads and returns `/spiffs/config.json` |
| `POST` | `/config` | Validates JSON, writes to `/spiffs/config.json` |
| `OPTIONS` | `/*` | CORS preflight response |

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `httpd_handle_t` global | Owned by `Webserver` class |
| Route registration in `app_main()` | `IWebServer::add_route()` called inside injected `setup_fn` |
| No interface — direct `esp_http_server` calls | `IWebServer` abstraction — mockable for tests |
| Manual `httpd_stop()` on cleanup | Destructor handles stop + deinit (RAII) |
