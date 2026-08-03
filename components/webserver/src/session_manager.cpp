/**
 * @file session_manager.cpp
 * @brief Implementation of SessionManager (port of reference session.c).
 */

#include "session_manager.hpp"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

namespace fpc {

static const char* TAG = "SESSION";

namespace {

/// Fill @p out (65 bytes) with 64 lowercase hex chars from 32 random bytes.
void rand_token_hex(char* out65)
{
    uint8_t raw[32];
    esp_fill_random(raw, sizeof(raw));
    for (int i = 0; i < 32; ++i) {
        std::snprintf(&out65[i * 2], 3, "%02x", raw[i]);
    }
    out65[kSessionTokenLen] = '\0';
}

} // namespace

void SessionManager::purge_expired()
{
    const int64_t now = esp_timer_get_time();
    for (auto& s : sessions_) {
        if (s.used && s.expires_us < now) {
            s.used = false;
        }
    }
}

Session* SessionManager::create(std::string_view user)
{
    purge_expired();

    // Reuse a free slot, else evict the one expiring soonest.
    Session* victim = nullptr;
    int64_t  oldest = LLONG_MAX;
    for (auto& s : sessions_) {
        if (!s.used) { victim = &s; break; }
        if (s.expires_us < oldest) { oldest = s.expires_us; victim = &s; }
    }
    if (victim == nullptr) { return nullptr; }

    *victim = Session{};
    victim->used = true;

    const std::size_t n = std::min(user.size(), kSessionUserMax - 1);
    std::memcpy(victim->username.data(), user.data(), n);
    victim->username[n] = '\0';

    rand_token_hex(victim->token.data());
    victim->expires_us = esp_timer_get_time() + kSessionLifetimeUs;
    return victim;
}

Session* SessionManager::find_by_token(std::string_view token)
{
    purge_expired();
    for (auto& s : sessions_) {
        if (s.used &&
            std::strncmp(s.token.data(), token.data(), kSessionTokenLen) == 0) {
            return &s;
        }
    }
    ESP_LOGI(TAG, "session not found");
    return nullptr;
}

Session* SessionManager::find_by_username(std::string_view username)
{
    for (auto& s : sessions_) {
        if (s.used &&
            std::strncmp(s.username.data(), username.data(), kSessionUserMax) == 0) {
            return &s;
        }
    }
    return nullptr;
}

void SessionManager::remove(Session* session)
{
    if (session == nullptr) {
        ESP_LOGE(TAG, "Attempted to remove a null session");
        return;
    }
    session->used = false;
    ESP_LOGI(TAG, "Session for user %s removed", session->username.data());
}

void SessionManager::clear()
{
    for (auto& s : sessions_) { s = Session{}; }
    ESP_LOGI(TAG, "All sessions cleared");
}

void SessionManager::set_cookie(httpd_req_t* req, const Session* session)
{
    if (req == nullptr || session == nullptr) { return; }
    // Static: httpd_resp_set_hdr stores the pointer (does not copy); the value
    // must outlive the handler's httpd_resp_send call.
    static char cookie[180];
    std::snprintf(cookie, sizeof(cookie),
                  "SID=%s; Path=/; HttpOnly; Max-Age=%d",
                  session->token.data(),
                  static_cast<int>(kSessionLifetimeUs / 1000000));
    ESP_LOGI(TAG, "Setting session cookie: %s", cookie);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
}

} // namespace fpc
