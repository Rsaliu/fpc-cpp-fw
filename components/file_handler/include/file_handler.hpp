/**
 * @file file_handler.hpp
 * @brief RAII file-system abstraction with injectable init/deinit.
 *
 * Architecture:
 *   IFileSystem        ← pure interface.
 *       └─ FileHandler ← concrete; injectable init/deinit via FileHandlerConfig.
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <string>
#include <functional>
#include <cstddef>
#include "common.hpp"

namespace fpc {

// ─── Interface ────────────────────────────────────────────────────────────────

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual Result<void>        write(const std::string& path,
                                      const std::string& data)         = 0;
    virtual Result<std::string> read(const std::string& path)          = 0;
    virtual Result<void>        append(const std::string& path,
                                       const std::string& data)        = 0;
    virtual Result<void>        rename(const std::string& src,
                                       const std::string& dst)         = 0;
    virtual Result<void>        remove(const std::string& path)        = 0;
    virtual Result<std::size_t> get_size(const std::string& path)      = 0;
};

// ─── Config ───────────────────────────────────────────────────────────────────

using FileInitFn   = std::function<Result<void>()>;
using FileDeinitFn = std::function<Result<void>()>;

struct FileHandlerConfig {
    FileInitFn   init_fn{};
    FileDeinitFn deinit_fn{};
};

// ─── Concrete implementation ──────────────────────────────────────────────────

class FileHandler final : public IFileSystem {
public:
    explicit FileHandler(FileHandlerConfig config);
    ~FileHandler() override;

    FileHandler(const FileHandler&)            = delete;
    FileHandler& operator=(const FileHandler&) = delete;

    [[nodiscard]] Result<void> init();
    [[nodiscard]] Result<void> deinit();
    [[nodiscard]] bool is_initialized() const noexcept;

    [[nodiscard]] Result<void>        write(const std::string& path,
                                             const std::string& data) override;
    [[nodiscard]] Result<std::string> read(const std::string& path) override;
    [[nodiscard]] Result<void>        append(const std::string& path,
                                              const std::string& data) override;
    [[nodiscard]] Result<void>        rename(const std::string& src,
                                              const std::string& dst) override;
    [[nodiscard]] Result<void>        remove(const std::string& path) override;
    [[nodiscard]] Result<std::size_t> get_size(const std::string& path) override;

private:
    FileHandlerConfig config_;
    bool              initialized_{false};
};

} // namespace fpc
