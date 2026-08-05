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

The main application registers these routes via a `WebserverSetupFn`. The helper
`make_webserver_routes(HandlerContext&)` builds this function and binds a
`HandlerContext*` as each handler's `user_ctx`.

| Method | Path | Handler | Auth |
|---|---|---|---|
| `OPTIONS` | `/*` | `cors_preflight_handler` | — |
| `GET` | `/reset` | `reset_handler` (clears sessions, credentials, config file) | — |
| `POST` | `/register` | `register_handler` (one-time registration) | — |
| `GET` | `/logout` | `logout_handler` | ✔ |
| `POST` | `/login` | `login_handler` | — |
| `GET` | `/config` | `get_config_handler` | ✔ |
| `POST` | `/config` | `set_config_handler` | ✔ |
| `GET` | `/*` | `rest_common_get_handler` (static files, registered **last**) | — |

Trailing `/` serves `/home_ui.html`. The static handler maps a path's extension to a
SPIFFS subfolder: `.html→html`, `.js→js`, `.css→css`, `.png/.ico/.svg→img`.

---

## Collaborating units

| Unit | Header | Responsibility |
|---|---|---|
| `SessionManager` / `ISessionManager` | `session_manager.hpp` | In-RAM session table (8 slots), token generation, cookies, expiry purge |
| `CredentialStore` / `ICredentialStore` | `credential_store.hpp` | NVS-backed PBKDF2-SHA256 credentials + registered flag; constant-time compare |
| Webserver utils | `webserver_utils.hpp` | body read, cookie parse, MIME, dir mapping, `{ "message": ... }` responses |
| `HandlerContext` | `handler_context.hpp` | Non-owning bundle (`WebserverContext*`, `ISessionManager*`, `ICredentialStore*`) passed via `req->user_ctx` |
| Handlers + routes | `webserver_handlers.hpp` | Route handlers, `auth_gate`, and `make_webserver_routes` factory |

**Ownership:** the owner (webserver task / `main`) creates the `SessionManager`,
`CredentialStore`, and `HandlerContext`, and must keep them alive for as long as the
server runs. `CredentialStore::init()`/`deinit()` are called by that owner (kept out of
`Webserver` to preserve dependency injection).

---

## `Webserver::init()` parity

`Webserver::init()` mounts SPIFFS and — when the corresponding config fields are set —
initialises **mDNS**, starts **NetBIOS**, and verifies WiFi is in **AP/APSTA** mode
(the server must run alongside the SoftAP). `deinit()` unwinds mDNS/NetBIOS/SPIFFS.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `httpd_handle_t` global | Owned by `Webserver` class |
| Route registration in `app_main()` | `make_webserver_routes()` → injected `setup_fn` |
| `setup_web_handlers()` | `make_webserver_routes(HandlerContext&)` |
| Raw `rest_server_context_t*` as `user_ctx` | `HandlerContext*` bundling all collaborators |
| Global `g_sessions[]` + free functions | `SessionManager` class (interface-backed) |
| `credential_store.c` free functions | `CredentialStore` class (interface-backed) |
| cJSON | `nlohmann::json` |
| No interface — direct `esp_http_server` calls | `IWebServer` abstraction — mockable for tests |
| Manual `httpd_stop()` on cleanup | Destructor handles stop + deinit (RAII) |

