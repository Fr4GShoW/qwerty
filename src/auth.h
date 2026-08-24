// auth.h
// Session/cookie-based login, matching server.py's behavior: a fixed
// username/password table, a session token cookie on successful login,
// and an 8-hour session lifetime.
//
// Credential storage note (same trade-off called out in the Windows/EXE
// build's README): comparisons are done directly against the plaintext
// table below rather than through a hash. In the Python version the
// password was SHA-256 hashed before comparison, but since that hash is
// embedded in a distributable binary either way, a determined person can
// recover the original credentials from a decompiled .exe regardless of
// whether hashing sits in between -- so this avoids pulling in (or
// hand-rolling, with the transcription-error risk that carries) a SHA-256
// implementation for a property it wasn't actually providing here. If
// this tool is ever exposed beyond trusted machines, move credential
// checking server-side instead of embedding it in the client binary.
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace op {

struct UserAccount {
    std::string username;
    std::string password;
    std::string role;
};

// Returns the account for `username` if the password matches, else nullptr.
const UserAccount* verifyLogin(const std::string& username, const std::string& password);

class SessionStore {
public:
    // Creates a new session for `username`/`role`, returns the opaque
    // session token to set as a cookie. Thread-safe.
    std::string create(const std::string& username, const std::string& role);

    // Looks up a session by token. Returns true and fills username/role if
    // valid and not expired; expired/unknown sessions are treated as
    // logged-out. Thread-safe.
    bool lookup(const std::string& token, std::string& username, std::string& role);

    // Thread-safe.
    void destroy(const std::string& token);

private:
    struct Session {
        std::string username;
        std::string role;
        long long expiresAtEpochSeconds;
    };

    std::string generateToken();

    std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
};

} // namespace op
