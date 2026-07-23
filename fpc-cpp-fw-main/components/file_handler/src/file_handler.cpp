#include "file_handler.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_log.h"

static const char* TAG = "FILE_HANDLER";

namespace fpc {

FileHandler::FileHandler(FileHandlerConfig config)
    : config_{std::move(config)}
{}

FileHandler::~FileHandler()
{
    if (initialized_) {
        (void)deinit();
    }
}

Result<void> FileHandler::init()
{
    ESP_LOGI(TAG, "Initialising file handler");
    if (config_.init_fn) {
        auto r = config_.init_fn();
        if (r.is_err()) { ESP_LOGE(TAG, "init_fn failed"); return r; }
    } else {
        ESP_LOGW(TAG, "No init_fn registered");
    }
    initialized_ = true;
    return Result<void>::ok();
}

Result<void> FileHandler::deinit()
{
    if (!initialized_) return Result<void>::err(SystemError::InvalidState);
    if (config_.deinit_fn) {
        auto r = config_.deinit_fn();
        if (r.is_err()) { ESP_LOGE(TAG, "deinit_fn failed"); return r; }
    }
    initialized_ = false;
    return Result<void>::ok();
}

bool FileHandler::is_initialized() const noexcept { return initialized_; }

Result<void> FileHandler::write(const std::string& path, const std::string& data)
{
    if (path.empty() || data.empty())
        return Result<void>::err(SystemError::InvalidParameter);

    FILE* f = fopen(path.c_str(), "w");
    if (!f) return Result<void>::err(SystemError::Failed);

    std::size_t written = fwrite(data.data(), 1, data.size(), f);
    fflush(f); fclose(f);
    if (written != data.size()) return Result<void>::err(SystemError::Failed);

    ESP_LOGI(TAG, "Written '%s' (%zu bytes)", path.c_str(), written);
    return Result<void>::ok();
}

Result<std::string> FileHandler::read(const std::string& path)
{
    if (path.empty()) return Result<std::string>::err(SystemError::InvalidParameter);

    FILE* f = fopen(path.c_str(), "r");
    if (!f) return Result<std::string>::err(SystemError::Failed);

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) { fclose(f); return Result<std::string>::err(SystemError::Failed); }
    if (sz == 0) { fclose(f); return Result<std::string>::ok(std::string{}); }

    std::string buf(static_cast<std::size_t>(sz), '\0');
    std::size_t actual = fread(&buf[0], 1, static_cast<std::size_t>(sz), f);
    fclose(f);

    if (actual != static_cast<std::size_t>(sz))
        return Result<std::string>::err(SystemError::Failed);

    ESP_LOGI(TAG, "Read '%s' (%zu bytes)", path.c_str(), actual);
    return Result<std::string>::ok(std::move(buf));
}

Result<void> FileHandler::append(const std::string& path, const std::string& data)
{
    if (path.empty() || data.empty())
        return Result<void>::err(SystemError::InvalidParameter);

    FILE* f = fopen(path.c_str(), "a");
    if (!f) return Result<void>::err(SystemError::Failed);

    std::size_t written = fwrite(data.data(), 1, data.size(), f);
    fflush(f); fclose(f);
    if (written != data.size()) return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

Result<void> FileHandler::rename(const std::string& src, const std::string& dst)
{
    if (src.empty() || dst.empty())
        return Result<void>::err(SystemError::InvalidParameter);
    if (::rename(src.c_str(), dst.c_str()) != 0)
        return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

Result<void> FileHandler::remove(const std::string& path)
{
    if (path.empty()) return Result<void>::err(SystemError::InvalidParameter);
    if (::remove(path.c_str()) != 0) return Result<void>::err(SystemError::Failed);
    return Result<void>::ok();
}

Result<std::size_t> FileHandler::get_size(const std::string& path)
{
    if (path.empty()) return Result<std::size_t>::err(SystemError::InvalidParameter);
    struct stat st{};
    if (stat(path.c_str(), &st) != 0)
        return Result<std::size_t>::err(SystemError::Failed);
    return Result<std::size_t>::ok(static_cast<std::size_t>(st.st_size));
}

} // namespace fpc
