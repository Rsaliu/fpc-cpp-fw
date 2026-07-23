/**
 * @file common.hpp
 * @brief Shared types, error codes, and utilities for the fpc-cpp project.
 *
 * This is a header-only component — nothing here generates object code.
 * All definitions use `inline` or are templates so that including this header
 * in multiple translation units does not cause multiple-definition errors.
 *
 * C++17 required (uses std::variant, std::string_view, Span<T>).
 * CXX_EXTENSIONS must be ON (enables GNU extensions).
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <optional>
#include <variant>
#include <functional>
#include <array>
#include <vector>

namespace fpc {

// ═══════════════════════════════════════════════════════════════════════════
// § 1  System-wide error codes
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief System-wide error codes (C++17 typed enum).
 *
 * Mirrors the reference project's `error_type_t` but expressed as an
 * `enum class` so that values cannot be silently converted to/from integers
 * and do not pollute the global namespace.
 */
enum class SystemError : uint8_t {
    Ok = 0,                  ///< No error.
    NullParameter,           ///< A required pointer argument was null.
    InvalidParameter,        ///< An argument had an out-of-range value.
    InvalidPinNumber,        ///< GPIO pin number is not valid for the platform.
    InvalidState,            ///< Operation not permitted in the current state.
    InvalidMode,             ///< Requested operating mode is not supported.
    InvalidBaudRate,         ///< Baud-rate value is not supported by hardware.
    InvalidLength,           ///< Buffer/message length is invalid.
    TimedOut,                ///< Operation exceeded its timeout.
    BufferOverflow,          ///< Destination buffer capacity exceeded.
    Unknown,                 ///< An undetermined error occurred.
    Failed,                  ///< Generic operation failure.
    NoResponse,              ///< Remote device did not respond.
    ChecksumValidationFailed,///< CRC / checksum mismatch detected.
    Busy,                    ///< Resource is temporarily unavailable.
    OperationFailed,         ///< Operation could not be completed.
    InvalidResponse,         ///< Response from remote device was malformed.
    OutOfRange,              ///< Value falls outside the allowed range.
};

/**
 * @brief Return a human-readable name for a SystemError value.
 *
 * Implemented as a `constexpr` function so the result can be evaluated at
 * compile time and avoids heap allocation (returns a `std::string_view`
 * backed by string-literal storage).
 *
 * @param err  The error code to describe.
 * @return A non-null-terminated view over a static string literal.
 */
[[nodiscard]] constexpr std::string_view to_string(SystemError err) noexcept
{
    switch (err) {
        case SystemError::Ok:                       return "Ok";
        case SystemError::NullParameter:            return "NullParameter";
        case SystemError::InvalidParameter:         return "InvalidParameter";
        case SystemError::InvalidPinNumber:         return "InvalidPinNumber";
        case SystemError::InvalidState:             return "InvalidState";
        case SystemError::InvalidMode:              return "InvalidMode";
        case SystemError::InvalidBaudRate:          return "InvalidBaudRate";
        case SystemError::InvalidLength:            return "InvalidLength";
        case SystemError::TimedOut:                 return "TimedOut";
        case SystemError::BufferOverflow:           return "BufferOverflow";
        case SystemError::Unknown:                  return "Unknown";
        case SystemError::Failed:                   return "Failed";
        case SystemError::NoResponse:               return "NoResponse";
        case SystemError::ChecksumValidationFailed: return "ChecksumValidationFailed";
        case SystemError::Busy:                     return "Busy";
        case SystemError::OperationFailed:          return "OperationFailed";
        case SystemError::InvalidResponse:          return "InvalidResponse";
        case SystemError::OutOfRange:               return "OutOfRange";
        default:                                    return "UnknownError";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// § 2  Result<T> — lightweight error-or-value return type
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Lightweight result type — holds either a success value or a
 *        SystemError, with no dynamic allocation.
 *
 * This pattern is used throughout the project instead of raw `esp_err_t`
 * returns or exceptions on hot paths.
 *
 * Usage example:
 * @code
 *   Result<int> read_sensor() {
 *       if (!ready) return Result<int>::err(SystemError::Busy);
 *       return Result<int>::ok(42);
 *   }
 *
 *   auto r = read_sensor();
 *   if (r.is_ok()) { process(r.value()); }
 *   else           { ESP_LOGE(TAG, "%s", fpc::to_string(r.error()).data()); }
 * @endcode
 *
 * @tparam T  The success value type.
 */
template<typename T>
class Result {
public:
    // ── Factory methods ───────────────────────────────────────────────────

    /// Create a successful Result wrapping @p value.
    [[nodiscard]] static Result ok(T value) { return Result{std::move(value)}; }

    /// Create an error Result carrying @p error.
    [[nodiscard]] static Result err(SystemError error) noexcept
    {
        return Result{error};
    }

    // ── Observers ─────────────────────────────────────────────────────────

    /// @return true if the Result holds a success value.
    [[nodiscard]] bool is_ok()  const noexcept
    {
        return std::holds_alternative<T>(m_data);
    }

    /// @return true if the Result holds an error code.
    [[nodiscard]] bool is_err() const noexcept
    {
        return std::holds_alternative<SystemError>(m_data);
    }

    // ── Value access ──────────────────────────────────────────────────────

    /// Access the success value (lvalue). Behaviour is undefined if is_err().
    [[nodiscard]] T&       value() &      { return std::get<T>(m_data); }
    /// @copydoc value()&
    [[nodiscard]] const T& value() const& { return std::get<T>(m_data); }
    /// Access the success value (rvalue, move-from). Behaviour is undefined if is_err().
    [[nodiscard]] T&&      value() &&
    {
        return std::get<T>(std::move(m_data));
    }

    /// @return The error code. Behaviour is undefined if is_ok().
    [[nodiscard]] SystemError error() const noexcept
    {
        return std::get<SystemError>(m_data);
    }

    /**
     * @brief Return the contained value, or @p fallback when this is an error.
     * @param fallback  Value to return if this result is an error.
     */
    [[nodiscard]] T value_or(T fallback) const noexcept
    {
        if (is_ok()) { return std::get<T>(m_data); }
        return fallback;
    }

private:
    explicit Result(T value)          : m_data{std::move(value)} {}
    explicit Result(SystemError err)  : m_data{err} {}

    std::variant<T, SystemError> m_data;
};

// ── Result<void> partial specialisation ──────────────────────────────────

/**
 * @brief Result specialisation for operations that succeed or fail without
 *        returning a value.
 *
 * Usage:
 * @code
 *   Result<void> init() {
 *       if (error_condition) return Result<void>::err(SystemError::Failed);
 *       return Result<void>::ok();
 *   }
 * @endcode
 */
template<>
class Result<void> {
public:
    [[nodiscard]] static Result ok()  noexcept { return Result{true}; }
    [[nodiscard]] static Result err(SystemError error) noexcept
    {
        return Result{error};
    }

    [[nodiscard]] bool        is_ok()  const noexcept { return m_ok; }
    [[nodiscard]] bool        is_err() const noexcept { return !m_ok; }
    [[nodiscard]] SystemError error()  const noexcept { return m_error; }

private:
    explicit Result(bool /*ok*/) noexcept
        : m_ok{true}, m_error{SystemError::Ok} {}
    explicit Result(SystemError error) noexcept
        : m_ok{false}, m_error{error} {}

    bool        m_ok;
    SystemError m_error;
};

// ═══════════════════════════════════════════════════════════════════════════
// § 3  Span<T> — C++17 non-owning view (std::span requires C++20)
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief C++17-compatible non-owning view over a contiguous sequence.
 *
 * Replaces std::span (C++20) so the codebase stays strictly C++17.
 * Constructors: pointer+size, C array, std::array, std::vector.
 */
template<typename T>
class Span {
public:
    using element_type = T;
    using value_type   = std::remove_cv_t<T>;
    using size_type    = std::size_t;
    using pointer      = T*;
    using reference    = T&;
    using iterator     = T*;

    constexpr Span() noexcept = default;

    constexpr Span(T* ptr, size_type count) noexcept
        : data_{ptr}, size_{count} {}

    template<std::size_t N>
    constexpr Span(T (&arr)[N]) noexcept
        : data_{arr}, size_{N} {}

    template<std::size_t N>
    constexpr Span(std::array<value_type, N>& arr) noexcept
        : data_{arr.data()}, size_{N} {}

    template<std::size_t N>
    constexpr Span(const std::array<value_type, N>& arr) noexcept
        : data_{arr.data()}, size_{N} {}

    Span(std::vector<value_type>& v) noexcept
        : data_{v.data()}, size_{v.size()} {}

    Span(const std::vector<value_type>& v) noexcept
        : data_{v.data()}, size_{v.size()} {}

    [[nodiscard]] constexpr pointer   data()  const noexcept { return data_; }
    [[nodiscard]] constexpr size_type size()  const noexcept { return size_; }
    [[nodiscard]] constexpr bool      empty() const noexcept { return size_ == 0; }
    [[nodiscard]] constexpr iterator  begin() const noexcept { return data_; }
    [[nodiscard]] constexpr iterator  end()   const noexcept { return data_ + size_; }
    [[nodiscard]] constexpr reference operator[](size_type i) const noexcept { return data_[i]; }

private:
    T*        data_{nullptr};
    size_type size_{0u};
};

// ═══════════════════════════════════════════════════════════════════════════
// § 4  Common type aliases
// ═══════════════════════════════════════════════════════════════════════════

/// Owning heap-allocated byte buffer.
using Bytes           = std::vector<uint8_t>;

/// Non-owning, read-only view over a contiguous byte sequence.
using ByteView        = Span<const uint8_t>;

/// Non-owning, mutable view over a contiguous byte sequence.
using MutableByteView = Span<uint8_t>;

// ═══════════════════════════════════════════════════════════════════════════
// § 4  System-wide constants
// ═══════════════════════════════════════════════════════════════════════════

/// Maximum RS485 frame payload (bytes).
inline constexpr std::size_t kMaxFramePayloadLength = 256U;

/// Maximum UART receive buffer (bytes).
inline constexpr std::size_t kMaxUartBufferSize      = 512U;

/// Default FreeRTOS task stack size for sensor/monitor tasks (bytes).
inline constexpr std::uint32_t kDefaultTaskStackSize  = 4096U;

/// Default FreeRTOS task priority.
inline constexpr std::uint8_t  kDefaultTaskPriority   = 5U;

/// Timeout used in blocking RTOS operations (ms).
inline constexpr std::uint32_t kDefaultTimeoutMs      = 1000U;

} // namespace fpc
