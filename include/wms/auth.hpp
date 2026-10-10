// auth.hpp - user accounts and password checks.
//
// The 2021 program compared typed passwords against string literals compiled
// into the binary ("Suppliaccess", ...). Accounts now live in data/users.csv
// and only a per-user random salt and SHA-256(salt + password) are stored:
//
//   username,role,salt,sha256
//   supplier,Supplier,3f9c...,a71e...
#pragma once

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "wms/permissions.hpp"

namespace wms {

struct User {
    std::string username;  // stored lower-case
    Role role = Role::Customer;
    std::string saltHex;   // 32 hex characters (16 random bytes)
    std::string hashHex;   // 64 hex characters
};

// SHA-256 over the salt (as its hex text) followed by the password.
std::string hashPassword(std::string_view saltHex, std::string_view password);

// 16 bytes from std::random_device, hex encoded.
std::string generateSalt();

// Compares two strings in time that depends only on their length, so the
// position of the first mismatching character is not leaked.
bool constantTimeEquals(std::string_view a, std::string_view b) noexcept;

class UserStore {
public:
    static constexpr std::size_t kMinPasswordLength = 8;

    // Creates a user with a fresh salt. Throws ValidationError for a bad or
    // duplicate username or a password shorter than kMinPasswordLength.
    const User& addUser(std::string_view username, Role role, std::string_view password);

    // Returns the user only if the username exists and the password matches.
    std::optional<User> authenticate(std::string_view username, std::string_view password) const;

    const User* find(std::string_view username) const noexcept;
    const std::vector<User>& users() const noexcept { return users_; }
    bool empty() const noexcept { return users_.empty(); }

    static UserStore readCsv(std::istream& in);
    void writeCsv(std::ostream& out) const;
    static UserStore loadCsv(const std::filesystem::path& file);  // empty if missing
    void saveCsv(const std::filesystem::path& file) const;

    // 3-32 characters from [A-Za-z0-9_.-]; returned lower-case.
    static std::string normalizeUsername(std::string_view raw);

private:
    std::vector<User> users_;
};

}  // namespace wms
