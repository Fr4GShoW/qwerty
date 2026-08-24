// auth.cpp
#include "auth.h"
#include <array>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace op {

namespace {
// Same two accounts as the original server.py USERS table.
const std::array<UserAccount, 2> kAccounts = {{
    {"admin", "AdminPassOM2026@!", "admin"},
    {"soc",   "SOCPASSOM2026@!",   "user"},
}};

constexpr long long kSessionLifetimeSeconds = 8LL * 60 * 60; // 8 hours, matches Flask's permanent_session_lifetime
}

const UserAccount* verifyLogin(const std::string& username, const std::string& password) {
    for (auto& acc : kAccounts) {
        if (acc.username == username && acc.password == password) {
            return &acc;
        }
    }
    return nullptr;
}

std::string SessionStore::generateToken() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 15);
    static const char* hex = "0123456789abcdef";
    std::string token;
    token.reserve(32);
    for (int i = 0; i < 32; ++i) token += hex[dist(rng)];
    return token;
}

std::string SessionStore::create(const std::string& username, const std::string& role) {
    std::string token = generateToken();
    long long now = static_cast<long long>(std::time(nullptr));

    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[token] = Session{username, role, now + kSessionLifetimeSeconds};
    return token;
}

bool SessionStore::lookup(const std::string& token, std::string& username, std::string& role) {
    if (token.empty()) return false;
    long long now = static_cast<long long>(std::time(nullptr));

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    if (it->second.expiresAtEpochSeconds < now) {
        sessions_.erase(it);
        return false;
    }
    username = it->second.username;
    role = it->second.role;
    return true;
}

void SessionStore::destroy(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}

} // namespace op
