# utils

**Type:** Library  
**Namespace:** `fpc::utils`  
**Depends on:** `common`

---

## What it does

`utils` provides **miscellaneous utility free-functions** that don't belong to any specific component. Currently it contains string helpers, JSON validation, and NVS (Non-Volatile Storage) blob helpers.

All functions live in `namespace fpc::utils` and return `Result<T>` — never raw `esp_err_t`.

---

## Public API

### String helpers

```cpp
namespace fpc::utils {

/**
 * Replace the first occurrence of `to_swap` in `input` with `replacement`.
 * Returns the modified string, or err() if `to_swap` is not found.
 */
[[nodiscard]] Result<std::string> swap_string(
    std::string_view input,
    std::string_view to_swap,
    std::string_view replacement);

}
```

**Example:**

```cpp
auto r = fpc::utils::swap_string("Hello, World!", "World", "ESP32");
// r.value() == "Hello, ESP32!"
```

---

### JSON validation

```cpp
/**
 * Validate that json_str is syntactically correct JSON (using cJSON internally).
 * Returns ok() if valid, err(SystemError::InvalidParameter) if malformed.
 */
[[nodiscard]] Result<void> is_valid_json(std::string_view json_str);
```

**Example:**

```cpp
if (fpc::utils::is_valid_json(user_input).is_err()) {
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "{\"error\":\"invalid_json\"}");
}
```

---

### NVS blob helpers

NVS (*Non-Volatile Storage*) is ESP-IDF's key-value store that survives reboots. These helpers wrap the ESP-IDF NVS API in `Result<T>`-returning functions.

```cpp
/**
 * Query the byte-size of a blob stored under key_name in handle.
 */
[[nodiscard]] Result<std::size_t> get_nvs_blob_size(
    nvs_handle_t handle,
    const char*  key_name);

/**
 * Read a blob from NVS and return it as a std::string.
 */
[[nodiscard]] Result<std::string> get_nvs_blob(
    nvs_handle_t handle,
    const char*  key_name,
    std::size_t  max_size);
```

**Example:**

```cpp
nvs_handle_t h;
nvs_open("storage", NVS_READONLY, &h);

auto size_r = fpc::utils::get_nvs_blob_size(h, "cert");
if (size_r.is_ok()) {
    auto blob_r = fpc::utils::get_nvs_blob(h, "cert", size_r.value());
}
nvs_close(h);
```

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `esp_err_t` returns | `Result<T>` — typed error, cannot be silently ignored |
| `char*` output parameter for blob | `Result<std::string>` — owned string returned by value |
| Loose helper functions with no namespace | `namespace fpc::utils` — no global namespace pollution |
