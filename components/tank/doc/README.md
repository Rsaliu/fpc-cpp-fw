# tank

**Type:** Library  
**Namespace:** `fpc`  
**Depends on:** `common`

---

## What it does

`tank` is the **tank data model**. It represents a single physical water tank as a C++ class with configuration, runtime state, and volume calculation methods. Like `pump`, it does not drive any hardware — it is a pure data model observed by `tank_monitor`.

---

## State machine

```
NotInitialized ──init()──► Initialized ──(monitor sets)──► Full / Low / Normal
              deinit() returns to NotInitialized from any state
```

```cpp
enum class TankState : uint8_t {
    NotInitialized = 0,
    Initialized    = 1,
    Normal         = 2,
    Full           = 3,
    Low            = 4,
};
```

---

## Tank shape

```cpp
enum class TankShape : uint8_t {
    Rectangle = 0,  // JSON: "RECTANGULAR"
    Cylinder  = 1,  // JSON: "CYLINDRICAL"
};

// Parse from string (used by ConfigManager):
std::optional<TankShape> shape_from_string(std::string_view s) noexcept;
```

---

## Public API

### `TankConfig`

```cpp
struct TankConfig {
    int32_t   id{-1};
    float     capacity_litres{0.0f};
    TankShape shape{TankShape::Rectangle};
    float     height_cm{0.0f};
    int32_t   full_level_mm{0};   // distance reading when tank is full
    int32_t   low_level_mm{0};    // distance reading when tank is critically low
};
```

---

### `Tank`

```cpp
class Tank {
public:
    explicit Tank(TankConfig config);

    Result<void> init();
    Result<void> deinit();

    // State transitions (called by TankMonitor):
    Result<void> set_normal();
    Result<void> set_full();
    Result<void> set_low();

    // Volume estimation from a level sensor reading (mm):
    Result<float> calculate_volume_litres(uint16_t level_mm) const noexcept;

    [[nodiscard]] TankState    state()         const noexcept;
    [[nodiscard]] int32_t      id()            const noexcept;
    [[nodiscard]] int32_t      full_level_mm() const noexcept;
    [[nodiscard]] int32_t      low_level_mm()  const noexcept;
    [[nodiscard]] float        capacity_litres() const noexcept;

    // Format a human-readable info string:
    Result<std::string> format_info() const;
};
```

---

## Volume calculation

For **rectangular** tanks:

$$V = \frac{\text{level\_mm}}{10} \times \frac{\text{capacity\_litres}}{\text{height\_cm}}$$

For **cylindrical** tanks, the calculation uses the tank's cross-sectional area derived from `height_cm` and `capacity_litres`.

---

## Level thresholds

| Config field | Meaning |
|---|---|
| `full_level_mm` | Distance reading (in mm) from sensor when tank is at the full mark |
| `low_level_mm` | Distance reading (in mm) from sensor when tank is at the critically low mark |

> Note: For ultrasonic sensors mounted at the top of the tank, a *smaller* distance reading means the water is *closer to the sensor* (higher water level). Ensure your `full_level_mm` / `low_level_mm` values reflect this.

---

## C → C++ conversion notes

| Old C (`fpc`) | New C++ (`fpc-cpp`) |
|---|---|
| `tank_state_t` plain C enum | `enum class TankState : uint8_t` |
| `tank_shape_t` plain C enum | `enum class TankShape : uint8_t` |
| `char* shape_name` with `strcmp` | `TankShape` enum + `shape_from_string()` returning `std::optional` |
| `error_type_t tank_calculate_volume(tank_t*, uint16_t, float*)` | `Result<float> Tank::calculate_volume_litres(uint16_t)` |
