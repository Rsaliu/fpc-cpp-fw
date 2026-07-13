/**
 * @file ota_messages.hpp
 * @brief OTA MQTT message model + standard JSON (de)serializers.
 *
 * Defines the OTA job / rollback data structures exchanged over MQTT and
 * exposes exception-free, Result-returning parse/serialize helpers built on
 * nlohmann/json. The wire format matches the legacy fpc C schema
 * (ota_job_info_t / ota_rollback_info_t) byte-for-byte.
 *
 * Job payload (all fields required):
 *   {
 *     "job_id":                "2025-07-10T12:00:00Z-abc123", // string, unique
 *     "version":               "1.4.2",                       // string
 *     "url":                   "https://host:8070/fw.bin",    // string, https URL
 *     "size":                  917504,                        // int, bytes
 *     "sha256":                "<64-hex-char digest>",        // string
 *     "signature":             "<base64 signature>",          // string
 *     "force":                 false,                         // bool
 *     "min_battery_percent":   20,                            // int
 *     "reboot_after_download": true                           // bool
 *   }
 *
 * Rollback payload (all fields required):
 *   {
 *     "job_id":           "2025-07-10T12:00:00Z-abc123", // string
 *     "failed_version":   "1.4.2",                       // string
 *     "restored_version": "1.4.1",                       // string
 *     "reason":           "sha256 mismatch"              // string
 *   }
 */

#pragma once

#include <string>
#include <string_view>
#include "common.hpp"

namespace fpc {
namespace serialization {

/// Parsed OTA firmware-update job (mirrors legacy ota_job_info_t).
struct OtaJobInfo {
    std::string job_id;
    std::string version;
    std::string url;
    int         size{0};
    std::string sha256;
    std::string signature;
    bool        force{false};
    int         min_battery_percent{0};
    bool        reboot_after_download{false};

    friend bool operator==(const OtaJobInfo& a, const OtaJobInfo& b) noexcept {
        return a.job_id == b.job_id && a.version == b.version && a.url == b.url &&
               a.size == b.size && a.sha256 == b.sha256 &&
               a.signature == b.signature && a.force == b.force &&
               a.min_battery_percent == b.min_battery_percent &&
               a.reboot_after_download == b.reboot_after_download;
    }
};

/// Parsed OTA rollback notification (mirrors legacy ota_rollback_info_t).
struct OtaRollbackInfo {
    std::string job_id;
    std::string failed_version;
    std::string restored_version;
    std::string reason;

    friend bool operator==(const OtaRollbackInfo& a, const OtaRollbackInfo& b) noexcept {
        return a.job_id == b.job_id && a.failed_version == b.failed_version &&
               a.restored_version == b.restored_version && a.reason == b.reason;
    }
};

// ─── Safe, exception-free public API ──────────────────────────────────────────
// All functions catch nlohmann exceptions internally and report via Result.

/// Parse + validate an OTA job payload. Missing/empty/wrong-typed fields → err.
[[nodiscard]] Result<OtaJobInfo> parse_ota_job(std::string_view json);

/// Serialize an OTA job to compact JSON.
[[nodiscard]] Result<std::string> serialize_ota_job(const OtaJobInfo& job);

/// Parse + validate an OTA rollback payload. Missing/empty fields → err.
[[nodiscard]] Result<OtaRollbackInfo> parse_ota_rollback(std::string_view json);

/// Serialize an OTA rollback to compact JSON.
[[nodiscard]] Result<std::string> serialize_ota_rollback(const OtaRollbackInfo& rollback);

} // namespace serialization
} // namespace fpc
