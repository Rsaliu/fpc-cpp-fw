# mqtt_conn

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`mqtt_conn` is a RAII wrapper around ESP-IDF's `esp-mqtt` client library. It manages the full MQTT connection lifecycle (connect, subscribe, publish, disconnect) and pushes incoming messages onto a caller-supplied **FreeRTOS queue** as `MqttMessage` structs.

`comms_manager` (the `RouteManager` class) sits on top of this queue and dispatches messages to topic handlers.

---

## Public API

### `MqttMessage` — message queued on arrival

```cpp
struct MqttMessage {
    char topic[128];
    char data[512];
};
```

Fixed-size arrays are used so the struct can be safely copied onto a FreeRTOS queue without dynamic allocation.

---

### `MqttConfig`

```cpp
struct MqttConfig {
    std::string   uri{};              // e.g. "mqtts://broker.example.com"
    int           port{8883};
    std::string   client_id{};
    const char*   ca_cert_pem{nullptr};      // TLS root CA (PEM)
    const char*   client_cert_pem{nullptr};  // TLS client certificate
    const char*   client_key_pem{nullptr};   // TLS client private key
    QueueHandle_t message_queue{nullptr};    // receives incoming MqttMessage
};
```

---

### `MqttConn`

```cpp
class MqttConn final {
public:
    explicit MqttConn(MqttConfig config);
    ~MqttConn();  // calls deinit() automatically

    Result<void> init();       // connects; blocks until connected or failed
    Result<void> deinit();

    Result<void> subscribe(const std::string& topic, int qos = 0);
    Result<void> unsubscribe(const std::string& topic);
    Result<void> publish(const std::string& topic,
                         const std::string& data,
                         int  qos    = 0,
                         bool retain = false);

    bool is_connected() const noexcept;
};
```

---

## Message flow

```
MQTT broker
     │
     │ (incoming publish on subscribed topic)
     ▼
MqttConn event_handler()
     │
     ▼
xQueueSend(message_queue, &msg, ...)
     │
     ▼
RouteManager::handle_request()   (in comms_manager)
     │
     ▼
RouteHandler(request_data)
```

---

## TLS support

All three PEM fields (`ca_cert_pem`, `client_cert_pem`, `client_key_pem`) are optional pointers to null-terminated PEM strings — typically stored in flash as embedded binary files. Set only `ca_cert_pem` for one-way TLS, or all three for mutual TLS.

---

## Connection synchronisation

`init()` uses an **ESP-IDF event group** internally to block the calling task until:
- `MQTT_EVENT_CONNECTED` fires → `init()` returns `Result<void>::ok()`
- `MQTT_EVENT_ERROR` fires → `init()` returns `Result<void>::err(SystemError::Failed)`

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `esp_mqtt_client_handle_t` stored as global | `void* client_` member — owned by `MqttConn` |
| Manual `esp_mqtt_client_destroy()` on error paths | Destructor calls `deinit()` automatically (RAII) |
| Callback context via `void*` | ESP-IDF event handler receives `this` pointer as `handler_args` |
