/**
 * @file webserver.hpp
 * @brief RAII HTTP server wrapper — IWebServer interface + Webserver concrete.
 *
 * Four-state machine: Uninitialized → Initialized → Running → Stopped
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <string>
#include <cstddef>
#include <functional>
#include "common.hpp"
#include "esp_http_server.h"

namespace fpc {

/// Maximum path length used for WebserverContext fields (mirrors ESP_VFS_PATH_MAX).
static constexpr std::size_t kWebserverMaxPathLen = 256U;
/// Scratch-buffer size for HTTP handler bodies.
static constexpr std::size_t kScratchBufSize      = 10240U;

// ─── Config ───────────────────────────────────────────────────────────────────

struct WebserverConfig {
    int         port{80};
    std::string document_root{};
    int         max_connections{7};
    std::string mdns_instance{};
    std::string mdns_hostname{};
    std::string base_path{};
    std::string web_mount_point{};
    std::string web_partition_label{};
    std::string config_file_path{};
};

/// Scratch-buffer context passed to HTTP handlers via req->user_ctx.
struct WebserverContext {
    char base_path[kWebserverMaxPathLen + 1];
    char scratch[kScratchBufSize];
    char config_file_path[kWebserverMaxPathLen + 1];
};

// ─── Interface ────────────────────────────────────────────────────────────────

class IWebServer {
public:
    virtual ~IWebServer() = default;

    virtual Result<void> init()   = 0;
    virtual Result<void> start()  = 0;
    virtual Result<void> stop()   = 0;
    virtual Result<void> deinit() = 0;

    virtual Result<void> add_route(httpd_uri_t* uri)                          = 0;
    virtual Result<void> remove_route(const char* uri, httpd_method_t method) = 0;

    [[nodiscard]] virtual httpd_handle_t    get_handle()  const noexcept = 0;
    [[nodiscard]] virtual WebserverContext* get_context() const noexcept = 0;
};

/// Callback invoked after the server starts to register HTTP routes.
using WebserverSetupFn = std::function<Result<void>(IWebServer&)>;

// ─── Concrete implementation ──────────────────────────────────────────────────

class Webserver final : public IWebServer {
public:
    explicit Webserver(WebserverConfig config);
    ~Webserver() override;

    Webserver(const Webserver&)            = delete;
    Webserver& operator=(const Webserver&) = delete;

    Result<void> init()   override;
    Result<void> start()  override;
    Result<void> stop()   override;
    Result<void> deinit() override;

    Result<void> add_route(httpd_uri_t* uri)                          override;
    Result<void> remove_route(const char* uri, httpd_method_t method) override;

    [[nodiscard]] httpd_handle_t    get_handle()  const noexcept override;
    [[nodiscard]] WebserverContext* get_context() const noexcept override;

private:
    enum class State : uint8_t {
        Uninitialized = 0, Initialized = 1, Running = 2, Stopped = 3,
    };

    WebserverConfig  config_;
    httpd_handle_t   server_{nullptr};
    WebserverContext context_{};
    State            state_{State::Uninitialized};
};

} // namespace fpc
