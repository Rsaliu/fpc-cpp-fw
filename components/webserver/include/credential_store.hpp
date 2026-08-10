/**
 * @file credential_store.hpp
 * @brief NVS-backed PBKDF2-SHA256 credential storage with a "registered" flag.
 *
 * Port of the reference C `credential_store.c`/`credential_store.h`.
 *
 * Architecture:
 *   ICredentialStore    ← pure interface (mockable in tests).
 *       └─ CredentialStore ← concrete; stores a single user record in NVS.
 *
 * Stored record layout (packed, blob in NVS namespace "secure"):
 *   ver | algo | iterations | salt[16] | hash[32] | username[32]
 *
 * C++17 / fpc-cpp port.
 */

#pragma once

#include <cstdint>
#include <string_view>
#include "common.hpp"

namespace fpc {

// ─── Constants (mirror reference #defines) ────────────────────────────────────
static constexpr const char* kAuthNamespace     = "secure";
static constexpr const char* kAuthKey           = "user_rec";
static constexpr const char* kRegisteredUserKey = "registered_user";
static constexpr std::size_t kUsernameMax       = 32U;
static constexpr std::size_t kSaltLen           = 16U;
static constexpr std::size_t kHashLen           = 32U;
static constexpr uint32_t    kPbkdf2Iters       = 10000U;
static constexpr uint8_t     kRegisteredFlag    = 0x69U;

/// Packed on-flash credential record.
struct __attribute__((packed)) AuthRecord {
    uint8_t  ver;
    uint8_t  algo;
    uint32_t iterations;
    uint8_t  salt[kSaltLen];
    uint8_t  hash[kHashLen];
    char     username[kUsernameMax];
};

// ─── Interface ────────────────────────────────────────────────────────────────

class ICredentialStore {
public:
    virtual ~ICredentialStore() = default;

    virtual Result<void>  init()                                           = 0;
    virtual Result<void>  deinit()                                         = 0;
    virtual Result<void>  set(std::string_view user, std::string_view pass)= 0;
    virtual bool          check(std::string_view user, std::string_view pass) = 0;
    virtual Result<bool>  is_registered()                                  = 0;
    virtual Result<void>  set_registered()                                 = 0;
    virtual Result<void>  clear()                                          = 0;
};

// ─── Concrete implementation ──────────────────────────────────────────────────

class CredentialStore final : public ICredentialStore {
public:
    CredentialStore() = default;
    ~CredentialStore() override = default;

    CredentialStore(const CredentialStore&)            = delete;
    CredentialStore& operator=(const CredentialStore&) = delete;

    Result<void> init()                                            override;
    Result<void> deinit()                                          override;
    Result<void> set(std::string_view user, std::string_view pass) override;
    bool         check(std::string_view user, std::string_view pass) override;
    Result<bool> is_registered()                                   override;
    Result<void> set_registered()                                  override;
    Result<void> clear()                                           override;
};

// ─── Free helpers (also unit-tested independently) ────────────────────────────

/// PBKDF2-HMAC-SHA256. Returns 0 on success (mbedtls convention).
int derive_hash(const uint8_t* pwd, std::size_t pwd_len,
                const uint8_t* salt, std::size_t salt_len,
                uint8_t out[kHashLen], uint32_t iters);

/// Constant-time comparison of @p n bytes.
bool ct_equal(const uint8_t* a, const uint8_t* b, std::size_t n);

} // namespace fpc
