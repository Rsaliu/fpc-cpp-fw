/**
 * @file session_manager.hpp
 * @brief In-RAM HTTP session table with token generation, cookies and expiry.
 *
 * Port of the reference C `session.c`/`session.h`. Owns a fixed-capacity
 * array of sessions (no heap). A `SessionManager` instance is created once and
 * shared with the HTTP handlers via `HandlerContext`.
 *
 * Architecture:
 *   ISessionManager    ← pure interface (mockable in tests).
 *       └─ SessionManager ← concrete; fixed std::array of slots.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include "esp_http_server.h"
#include "common.hpp"

namespace fpc {

/// Maximum number of concurrent sessions.
static constexpr std::size_t kMaxSessions      = 8U;
/// Maximum username length stored in a session (includes NUL).
static constexpr std::size_t kSessionUserMax   = 32U;
/// Session token length in hex characters (32 random bytes → 64 hex chars).
static constexpr std::size_t kSessionTokenLen  = 64U;
/// Session lifetime in microseconds (1 hour).
static constexpr int64_t     kSessionLifetimeUs = 60LL * 60 * 1000000;

/// A single session slot.
struct Session {
    bool                                    used{false};
    std::array<char, kSessionUserMax>       username{};
    std::array<char, kSessionTokenLen + 1>  token{};
    int64_t                                 expires_us{0};
};

// ─── Interface ────────────────────────────────────────────────────────────────

class ISessionManager {
public:
    virtual ~ISessionManager() = default;

    /// Create (or recycle) a session for @p user. Returns nullptr on failure.
    virtual Session* create(std::string_view user)                = 0;
    /// Find a live session by its token, or nullptr.
    virtual Session* find_by_token(std::string_view token)        = 0;
    /// Find a live session by username, or nullptr.
    virtual Session* find_by_username(std::string_view username)  = 0;
    /// Mark a session unused. Null-safe.
    virtual void     remove(Session* session)                     = 0;
    /// Clear every session slot.
    virtual void     clear()                                      = 0;
    /// Write the `Set-Cookie` header for @p session onto @p req.
    virtual void     set_cookie(httpd_req_t* req, const Session* session) = 0;
};

// ─── Concrete implementation ──────────────────────────────────────────────────

class SessionManager final : public ISessionManager {
public:
    SessionManager() = default;
    ~SessionManager() override = default;

    SessionManager(const SessionManager&)            = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    Session* create(std::string_view user)               override;
    Session* find_by_token(std::string_view token)       override;
    Session* find_by_username(std::string_view username) override;
    void     remove(Session* session)                    override;
    void     clear()                                     override;
    void     set_cookie(httpd_req_t* req, const Session* session) override;

private:
    void purge_expired();

    std::array<Session, kMaxSessions> sessions_{};
};

} // namespace fpc
