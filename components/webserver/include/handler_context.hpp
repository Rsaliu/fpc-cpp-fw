/**
 * @file handler_context.hpp
 * @brief Aggregates the collaborators every HTTP handler needs.
 *
 * A single `HandlerContext` is created by the route-setup factory and passed as
 * `req->user_ctx` to every registered handler. It is a non-owning view: the
 * caller (typically the webserver task) owns the referenced objects and must
 * keep them alive for the lifetime of the running server.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include "webserver.hpp"          // WebserverContext
#include "session_manager.hpp"    // ISessionManager
#include "credential_store.hpp"   // ICredentialStore

namespace fpc {

/// Non-owning bundle of handler collaborators (passed via req->user_ctx).
struct HandlerContext {
    WebserverContext* ctx{nullptr};       ///< scratch buffer + base/config paths.
    ISessionManager*  sessions{nullptr};  ///< session table.
    ICredentialStore* creds{nullptr};     ///< credential store.
};

} // namespace fpc
