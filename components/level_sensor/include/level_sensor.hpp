/**
 * @file level_sensor.hpp
 * @brief RS-485 level-sensor abstraction — ILevelSensor interface +
 *        concrete LevelSensor implementation using std::function callbacks.
 *
 * Architecture:
 *
 *   ILevelSensor          ← pure interface; consumers (tank_monitor) depend
 *       └─ LevelSensor    ← concrete; wired at construction time with three
 *                           typed std::function callbacks:
 *                            • FrameBuilder  — builds the request frame.
 *                            • Transport     — sends frame, receives response.
 *                            • Interpreter   — decodes raw response bytes.
 *
 * Why std::function instead of virtual dispatch?
 *   The reference used raw `void*` function-pointer callbacks.  Using
 *   `std::function<>` gives type safety, captures (so lambdas work), and
 *   eliminates the `void* context` anti-pattern entirely.
 *
 * Dependencies:
 *   - `protocol` component for the GL-A01 frame builder & interpreter types.
 *   - `common` for ByteView / Result<T>.
 *   - Callers inject an `Rs485` (or any mock) via the Transport callback.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include "common.hpp"
#include "protocol.hpp"   // protocol::gl_a01::RequestFrame, interpret_response

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  Callback type aliases
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Callback that builds a request frame for a given slave address.
 *
 * Signature: `RequestFrame(uint8_t slave_addr)`
 *
 * Example binding (GL-A01 level):
 * @code
 *   LevelSensor::FrameBuilder fb =
 *       [](uint8_t addr){ return protocol::gl_a01::build_read_level(addr); };
 * @endcode
 */
using FrameBuilder = std::function<protocol::gl_a01::RequestFrame(uint8_t)>;

/**
 * @brief Callback that sends a request frame and receives a raw response.
 *
 * Signature: `Result<void>(ByteView request, MutableByteView response_buf,
 *                          uint32_t timeout_ms, int32_t& bytes_read)`
 *
 * Example binding (production Rs485):
 * @code
 *   LevelSensor::Transport tr =
 *       [&bus](ByteView req, MutableByteView resp,
 *              uint32_t tms, int32_t& n)
 *       { return bus.send_receive(req, resp, tms, n); };
 * @endcode
 */
using Transport = std::function<Result<void>(ByteView,
                                             MutableByteView,
                                             uint32_t,
                                             int32_t&)>;

/**
 * @brief Callback that decodes a raw response frame into a 16-bit sensor value.
 *
 * Signature: `Result<uint16_t>(ByteView response)`
 */
using ResponseInterpreter = std::function<Result<uint16_t>(ByteView)>;

// ═══════════════════════════════════════════════════════════════════════════
// § 2  LevelSensorConfig
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Level-sensor configuration set at construction time.
 */
struct LevelSensorConfig {
    int32_t             id{-1};           ///< Application-level identifier (>= 0).
    uint8_t             sensor_addr{0x01};///< Modbus slave address of the sensor.
    FrameBuilder        frame_builder{};  ///< Builds the request frame.
    Transport           transport{};      ///< Sends frame / receives response.
    ResponseInterpreter interpreter{};    ///< Decodes the response bytes.
    uint32_t            timeout_ms{100u}; ///< Receive timeout in milliseconds.
    uint32_t            blindspot_mm{28}; ///< The minimium range(Blindspot) of the sensor

};

// ═══════════════════════════════════════════════════════════════════════════
// § 3  ILevelSensor — interface for consumers
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Pure interface for a level sensor.
 *
 * Tank monitors depend only on this interface, making them testable without
 * real hardware or a real `LevelSensor` instance.
 */
class ILevelSensor {
public:
    virtual ~ILevelSensor() = default;

    /// Initialise the sensor. Must be called before `read()`.
    virtual Result<void>     init()   = 0;

    /// Release resources.
    virtual Result<void>     deinit() = 0;

    /**
     * @brief Trigger a measurement and return the raw level in millimetres.
     *
     * Internally: build request frame → transport → interpret response.
     */
    virtual Result<uint16_t> read()   = 0;

    /// Return the sensor's application-level ID.
    virtual int32_t          id()     const = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 4  LevelSensor — concrete implementation
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Concrete RS-485 level sensor using injected std::function callbacks.
 *
 * Non-copyable (represents a unique sensor channel), movable.
 */
class LevelSensor final : public ILevelSensor {
public:
    /**
     * @param config  Sensor configuration (moved in).
     *
     * Construction fails silently only if callbacks are null — `init()` will
     * then return `InvalidParameter`.
     */
    explicit LevelSensor(LevelSensorConfig config) noexcept;

    LevelSensor(const LevelSensor&)            = delete;
    LevelSensor& operator=(const LevelSensor&) = delete;
    LevelSensor(LevelSensor&&)                 = default;
    LevelSensor& operator=(LevelSensor&&)      = default;
    ~LevelSensor() override                    = default;

    Result<void>     init()   override;
    Result<void>     deinit() override;
    Result<uint16_t> read()   override;
    int32_t          id()     const override;

private:
    LevelSensorConfig m_config;
    bool              m_active{false};

    /// Internal receive buffer — stack-allocated, fixed size.
    static constexpr std::size_t kRxBufSize = 64u;
};

} // namespace fpc
