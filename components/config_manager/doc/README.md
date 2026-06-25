# config_manager

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`, `setup_config`, `json` (ESP-IDF cJSON)

---

## What it does

`config_manager` is the JSON parsing layer. It converts a raw JSON string — either loaded from a SPIFFS file or provided directly — into a fully populated `AppSetupConfig` struct (defined in `setup_config`). The result is then passed to `Factory` to wire the live component graph.

It is a pure utility class: all methods are `static`, construction is deleted, and it holds no state.

---

## Public API

```cpp
class ConfigManager final {
public:
    ConfigManager() = delete;  // static-only class

    // Parse a JSON string into AppSetupConfig.
    // Returns SystemError::InvalidParameter if JSON is malformed or fields are missing.
    [[nodiscard]] static Result<AppSetupConfig>
    parse(std::string_view json_str) noexcept;

    // Read a file from SPIFFS (or any mounted filesystem) into a string.
    // Returns SystemError::Failed if the file cannot be opened.
    [[nodiscard]] static Result<std::string>
    read_file(const char* path) noexcept;
};
```

---

## Typical usage

```cpp
// Option A — load from SPIFFS
auto file_result = ConfigManager::read_file("/spiffs/config.json");
if (file_result.is_err()) {
    // file not found — use fallback
}
auto cfg_result = ConfigManager::parse(file_result.value());

// Option B — parse directly from a string
auto cfg_result = ConfigManager::parse(kDefaultJson);

if (cfg_result.is_ok()) {
    // Pass to Factory:
    auto app = Factory::create_from_config(cfg_result.value().pump_control_units[0]);
}
```

---

## JSON schema

See the [project README](../../../../README.md#6-configuration) for the full JSON schema. The top-level fields are:

```json
{
  "site_id":           "string",
  "device_id":         "string",
  "pump_control_units": [ { ... } ]
}
```

`parse()` uses ESP-IDF's built-in **cJSON** library internally. All required fields are validated; missing or wrongly-typed fields cause `SystemError::InvalidParameter`.

---

## Pipeline position

```
/spiffs/config.json  ──read_file()──►  std::string
                                             │
                                         parse()
                                             │
                                       AppSetupConfig  ──►  Factory
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `config_manager_parse(const char*, config_t*)` with `error_type_t` return | `ConfigManager::parse(std::string_view)` returning `Result<AppSetupConfig>` — value and error bundled together |
| Caller-allocated output struct passed by pointer | Value returned by `Result<T>` — no output parameters |
| Raw `FILE*` operations | `read_file()` wraps `fopen`/`fread`/`fclose` and returns `Result<std::string>` |
