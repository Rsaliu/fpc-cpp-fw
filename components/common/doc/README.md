# common

**Type:** Header-only  
**Namespace:** `fpc`  
**Depends on:** *(nothing — this is the foundation)*

---

## What it does

`common` is the shared foundation for the entire `fpc-cpp` project. It provides the types, error codes, and small utilities that every other component relies on. Because it is header-only (no `.cpp` file), including it costs nothing at link time.

---

## Public API

### `SystemError` — typed error codes

```cpp
enum class SystemError : uint8_t {
    Ok,
    NullParameter,
    InvalidParameter,
    InvalidPinNumber,
    InvalidState,
    Failed,
    TimedOut,
    // ... and more
};

constexpr std::string_view to_string(SystemError err) noexcept;
```

An `enum class` (scoped enum) — values cannot be accidentally compared to raw integers. `to_string()` is `constexpr` so it can be used in log messages with zero heap allocation.

---

### `Result<T>` — error-or-value return type

```cpp
template<typename T>
class Result {
public:
    static Result ok(T value);
    static Result err(SystemError error);

    bool      is_ok()    const noexcept;
    bool      is_err()   const noexcept;
    T&        value();          // asserts is_ok()
    SystemError error() const;  // asserts is_err()
};

// Specialisation for functions that succeed-or-fail with no return value:
template<>
class Result<void> { ... };
```

Every fallible function in the project returns `Result<T>` instead of raw `esp_err_t`. This makes it impossible to silently ignore an error — if you call `value()` on a failed result, you get an assertion failure immediately rather than undefined behaviour later.

**Example:**

```cpp
Result<float> reading = sensor.read();
if (reading.is_ok()) {
    float amps = reading.value();
} else {
    ESP_LOGE(TAG, "Sensor error: %s", fpc::to_string(reading.error()).data());
}
```

---

### `Span<T>` and `ByteView` / `MutableByteView`

```cpp
template<typename T>
class Span {
public:
    Span(T* data, std::size_t size);
    T*          data()  const noexcept;
    std::size_t size()  const noexcept;
    T&          operator[](std::size_t i);
};

using ByteView        = Span<const uint8_t>;
using MutableByteView = Span<uint8_t>;
using Bytes           = std::vector<uint8_t>;
```

`Span<T>` is a non-owning view over a contiguous sequence — similar to C++20's `std::span`. It replaces raw `uint8_t* + size_t` pairs throughout the driver APIs.

---

### `hardware_pins.hpp` — board-level pin constants

```cpp
namespace fpc::board {
    inline constexpr gpio_num_t kAds1115SdaPin     = GPIO_NUM_6;
    inline constexpr gpio_num_t kAds1115SclPin     = GPIO_NUM_7;
    inline constexpr gpio_num_t kLevelSensorRs485TxPin  = GPIO_NUM_10;
    inline constexpr gpio_num_t kLevelSensorRs485RxPin  = GPIO_NUM_11;
    inline constexpr gpio_num_t kLevelSensorRs485DirPin = GPIO_NUM_9;
    // ...
}
```

All non-configurable hardware pin assignments live here as `inline constexpr` values — a single source of truth for physical wiring.

---

## C++17 features used

| Feature | Where | Benefit |
|---|---|---|
| `enum class` | `SystemError` | Scoped names, no implicit int conversion |
| `std::variant` | `Result<T>` internals | Holds either T or SystemError, never both |
| `std::string_view` | `to_string()` returns | No heap allocation for string constants |
| `inline constexpr` | `hardware_pins.hpp` | Single definition across all TUs |
| `[[nodiscard]]` | All return values | Compiler warns if result is discarded |

---

## Build notes

```cmake
idf_component_register(
    INCLUDE_DIRS "include"
    # No SRCS — header-only
)
target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_17)
target_compile_options(${COMPONENT_LIB} INTERFACE -std=gnu++17)
```

The C++17 requirement is propagated to every component that `REQUIRES common`, so you only need to set it once here.
