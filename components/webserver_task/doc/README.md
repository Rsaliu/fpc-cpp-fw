# webserver_task

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `webserver`

---

## What it does

`webserver_task` wraps the `Webserver` class in a **FreeRTOS task** and manages its full lifecycle (init → start → run → stop → deinit). It accepts an optional `WebserverSetupFn` callback that is invoked after the server starts, giving the caller a chance to register HTTP routes.

This decouples "how long the server runs" (this component) from "what routes the server serves" (the injected setup function in `main.cpp`).

---

## Public API

### `WebserverSetupFn`

```cpp
using WebserverSetupFn = std::function<Result<void>(IWebServer&)>;
```

Called once after `Webserver::start()` succeeds. Use it to register all HTTP routes:

```cpp
ws_cfg.setup_fn = [](IWebServer& server) -> Result<void> {
    static httpd_uri_t health_uri{ .uri = "/health", .method = HTTP_GET,
                                   .handler = health_handler };
    return server.add_route(&health_uri);
};
```

---

### `WebserverTaskConfig`

```cpp
struct WebserverTaskConfig {
    WebserverConfig  webserver_config{};  // passed to Webserver constructor
    WebserverSetupFn setup_fn{};          // optional route registration callback
    uint32_t         stack_size{8192};
    UBaseType_t      priority{5};
};
```

---

### `WebserverTask`

```cpp
class WebserverTask final {
public:
    explicit WebserverTask(WebserverTaskConfig config);
    ~WebserverTask();  // calls stop() if running (RAII)

    Result<void> start();
    Result<void> stop();
    bool         is_running() const noexcept;
};
```

---

## Task lifecycle

```
WebserverTask::start()
    │
    └── xTaskCreate(task_fn, ...)
              │
         Webserver server{config.webserver_config};
         server.init()
         server.start()
         config.setup_fn(server)   ← registers routes
              │
         while (running_) {
             vTaskDelay(pdMS_TO_TICKS(100));
         }
              │
         server.stop()
         server.deinit()

WebserverTask::stop()  →  running_ = false  →  task exits
~WebserverTask()       →  calls stop() if still running
```

---

## Usage (from `main.cpp`)

```cpp
WebserverTaskConfig ws_cfg{};
ws_cfg.webserver_config.port              = 80;
ws_cfg.webserver_config.mdns_hostname     = "fpc-webserver";
ws_cfg.webserver_config.base_path         = "/spiffs";
ws_cfg.webserver_config.config_file_path  = "config.json";
ws_cfg.setup_fn = [](IWebServer& server) -> Result<void> {
    // register /health, /config GET, /config POST, /* OPTIONS
    return Result<void>::ok();
};

g_web_runtime.web_task = std::make_unique<WebserverTask>(std::move(ws_cfg));
g_web_runtime.web_task->start();
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `setup_web_handlers()` called in a fixed place in `app_main` | `WebserverSetupFn` injected at construction time |
| `httpd_handle_t` global managed manually | `Webserver` object owned inside task, destroyed on task exit |
| Separate init/start/stop calls scattered across app code | `WebserverTask::start()` / `stop()` encapsulates all lifecycle steps |
