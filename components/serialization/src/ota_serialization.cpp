/**
 * @file ota_serialization.cpp
 * @brief nlohmann/json (de)serializers for the OTA message model.
 *
 * ADL to_json/from_json hooks provide the idiomatic nlohmann mapping; the
 * public Result-returning wrappers guarantee no exception escapes and enforce
 * the required-field / non-empty-string validation of the legacy C schema.
 */

#include "ota_messages.hpp"

#include <initializer_list>
#include <nlohmann/json.hpp>
#include "esp_log.h"

namespace fpc {
namespace serialization {

namespace {
constexpr const char* TAG = "OTA_SERIALIZATION";

// Return err(InvalidParameter) if any listed field is an empty string.
Result<void> require_non_empty(std::initializer_list<const std::string*> fields,
                               std::initializer_list<const char*> names) {
    auto name = names.begin();
    for (const std::string* f : fields) {
        if (f->empty()) {
            ESP_LOGE(TAG, "OTA message: field '%s' is empty", *name);
            return Result<void>::err(SystemError::InvalidParameter);
        }
        ++name;
    }
    return Result<void>::ok();
}
} // namespace

// ─── ADL hooks: OtaJobInfo ────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const OtaJobInfo& v) {
    j = nlohmann::json{
        {"job_id",                v.job_id},
        {"version",               v.version},
        {"url",                   v.url},
        {"size",                  v.size},
        {"sha256",                v.sha256},
        {"signature",             v.signature},
        {"force",                 v.force},
        {"min_battery_percent",   v.min_battery_percent},
        {"reboot_after_download", v.reboot_after_download},
    };
}

void from_json(const nlohmann::json& j, OtaJobInfo& v) {
    j.at("job_id").get_to(v.job_id);
    j.at("version").get_to(v.version);
    j.at("url").get_to(v.url);
    j.at("size").get_to(v.size);
    j.at("sha256").get_to(v.sha256);
    j.at("signature").get_to(v.signature);
    j.at("force").get_to(v.force);
    j.at("min_battery_percent").get_to(v.min_battery_percent);
    j.at("reboot_after_download").get_to(v.reboot_after_download);
}

// ─── ADL hooks: OtaRollbackInfo ───────────────────────────────────────────────

void to_json(nlohmann::json& j, const OtaRollbackInfo& v) {
    j = nlohmann::json{
        {"job_id",           v.job_id},
        {"failed_version",   v.failed_version},
        {"restored_version", v.restored_version},
        {"reason",           v.reason},
    };
}

void from_json(const nlohmann::json& j, OtaRollbackInfo& v) {
    j.at("job_id").get_to(v.job_id);
    j.at("failed_version").get_to(v.failed_version);
    j.at("restored_version").get_to(v.restored_version);
    j.at("reason").get_to(v.reason);
}

// ─── Public safe API ──────────────────────────────────────────────────────────

Result<OtaJobInfo> parse_ota_job(std::string_view json) {
    try {
        auto j = nlohmann::json::parse(json);
        auto job = j.get<OtaJobInfo>();
        if (auto v = require_non_empty(
                {&job.job_id, &job.version, &job.url, &job.sha256, &job.signature},
                {"job_id", "version", "url", "sha256", "signature"});
            v.is_err()) {
            return Result<OtaJobInfo>::err(v.error());
        }
        return Result<OtaJobInfo>::ok(std::move(job));
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "parse_ota_job failed: %s", e.what());
        return Result<OtaJobInfo>::err(SystemError::InvalidParameter);
    }
}

Result<std::string> serialize_ota_job(const OtaJobInfo& job) {
    try {
        nlohmann::json j = job;
        return Result<std::string>::ok(j.dump());
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "serialize_ota_job failed: %s", e.what());
        return Result<std::string>::err(SystemError::Failed);
    }
}

Result<OtaRollbackInfo> parse_ota_rollback(std::string_view json) {
    try {
        auto j = nlohmann::json::parse(json);
        auto info = j.get<OtaRollbackInfo>();
        if (auto v = require_non_empty(
                {&info.job_id, &info.failed_version, &info.restored_version, &info.reason},
                {"job_id", "failed_version", "restored_version", "reason"});
            v.is_err()) {
            return Result<OtaRollbackInfo>::err(v.error());
        }
        return Result<OtaRollbackInfo>::ok(std::move(info));
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "parse_ota_rollback failed: %s", e.what());
        return Result<OtaRollbackInfo>::err(SystemError::InvalidParameter);
    }
}

Result<std::string> serialize_ota_rollback(const OtaRollbackInfo& rollback) {
    try {
        nlohmann::json j = rollback;
        return Result<std::string>::ok(j.dump());
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "serialize_ota_rollback failed: %s", e.what());
        return Result<std::string>::err(SystemError::Failed);
    }
}

} // namespace serialization
} // namespace fpc
