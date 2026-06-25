# comms_manager

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `mqtt_conn`

---

## What it does

`comms_manager` is the MQTT message routing layer. It sits on top of `mqtt_conn` (which handles the raw MQTT connection) and provides a **topic-to-handler dispatch table** — the `RouteManager` class.

When an MQTT message arrives, `RouteManager::handle_request()` pops it from a shared FreeRTOS queue and calls the registered handler for that topic. This is the C++ equivalent of the reference project's `route_manager.h` / `route_manager.c`.

---

## Public API

### `RouteHandler` — callback type

```cpp
using RouteHandler = std::function<void(const std::string& request_data)>;
```

Any callable that accepts the raw message payload string as a `const std::string&`.

---

### `RouteManagerConfig`

```cpp
struct RouteManagerConfig {
    QueueHandle_t message_queue{nullptr};  // FreeRTOS queue fed by MqttConn
};
```

---

### `RouteManager`

```cpp
class RouteManager final {
public:
    static constexpr std::size_t kMaxRoutes = 100U;

    explicit RouteManager(RouteManagerConfig config);

    Result<void> init();

    // Register a handler for a specific MQTT topic string
    Result<void> add_route(const std::string& topic, RouteHandler handler);

    // Pop one message from the queue and dispatch to its handler
    Result<void> handle_request();

    bool        is_initialized() const noexcept;
    std::size_t route_count()    const noexcept;
};
```

---

## How it fits together

```
MqttConn  ──pushes MqttMessage──►  QueueHandle_t
                                         │
                              RouteManager::handle_request()
                                         │
                          std::unordered_map<topic, RouteHandler>
                                         │
                              RouteHandler(request_data)
```

`RouteManager` uses `std::unordered_map` for O(1) average-case topic lookup regardless of how many routes are registered (up to `kMaxRoutes`).

---

## Example usage

```cpp
MqttConfig mqtt_cfg{.uri = "mqtt://broker.local", .message_queue = queue};
MqttConn   mqtt{mqtt_cfg};
mqtt.init();
mqtt.subscribe("/fpc/command");

RouteManagerConfig rm_cfg{.message_queue = queue};
RouteManager rm{rm_cfg};
rm.init();

rm.add_route("/fpc/command", [](const std::string& data) {
    ESP_LOGI("CMD", "Received: %s", data.c_str());
});

// In a polling loop or task:
while (true) {
    rm.handle_request();  // blocks until a message arrives or times out
}
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| Function-pointer dispatch table (array of struct) | `std::unordered_map<std::string, RouteHandler>` |
| `void*` callback context | Lambda capture — no context pointer needed |
| Manual array bounds check | `kMaxRoutes` enforced by `add_route()` returning `Result<void>` |
