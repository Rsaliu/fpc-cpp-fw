# serialization

**Type:** Library (placeholder)  
**Namespace:** `fpc::serialization`  
**Depends on:** `common`

---

## What it does

`serialization` is a **placeholder component** that reserves a namespace for future serialisation and deserialisation helpers. The original C project had empty `serializers.h` / `deserializers.h` headers; this component preserves that slot in the build graph so that other components can add `REQUIRES "serialization"` now without changes later.

---

## Current state

The header currently contains only the namespace declaration:

```cpp
namespace fpc {
namespace serialization {

// Placeholder — add serialize/deserialize functions here as needed.

} // namespace serialization
} // namespace fpc
```

No implementation file exists yet.

---

## Planned use

Future serialisation functions will convert between in-memory C++ structs and wire formats (e.g. MQTT payloads, binary protocol frames, CBOR). Candidate functions to add here:

```cpp
// Serialize an AppSetupConfig to a JSON string
Result<std::string> to_json(const AppSetupConfig& cfg);

// Serialize a MonitorEvent to a compact binary payload
Result<Bytes> to_wire(const MonitorEvent& event);
```

---

## Adding functions

1. Declare new functions in `include/serialization.hpp` inside `namespace fpc::serialization`.
2. Implement them in `src/serialization.cpp`.
3. Update `CMakeLists.txt` to add `src/serialization.cpp` to `SRCS`.
4. Add any new dependencies to the `REQUIRES` list.

No other components need to change — they already `REQUIRES serialization`.
