#include "wms/auth.hpp"

#include <array>
#include <cctype>
#include <fstream>
#include <istream>
#include <ostream>
#include <random>
#include <system_error>

#include "wms/errors.hpp"
#include "wms/sha256.hpp"
#include "wms/text.hpp"

namespace wms {

namespace {

constexpr const char* kHeader = "username,role,salt,sha256";

bool isHex(std::string_view s, std::size_t expectedLength) noexcept {
    if (s.size() != expectedLength) return false;
    for (char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

}  // namespace

std::string hashPassword(std::string_view saltHex, std::string_view password) {
    Sha256 h;
    h.update(saltHex);
    h.update(password);
    const auto d = h.finish();
    return toHex(d.data(), d.size());
}

std::string generateSalt() {
    std::random_device rd;
    std::array<std::uint8_t, 16> bytes{};
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
        const std::uint32_t r = rd();
        for (std::size_t j = 0; j < 4; ++j) bytes[i + j] = static_cast<std::uint8_t>(r >> (8 * j));
    }
    return toHex(bytes.data(), bytes.size());
}

bool constantTimeEquals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

std::string UserStore::normalizeUsername(std::string_view raw) {
    const std::string name = text::toLower(text::trim(raw));
    if (name.size() < 3 || name.size() > 32) {
        throw ValidationError("username must be 3-32 characters");
    }
    for (char c : name) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' ||
                        c == '-';
        if (!ok) throw ValidationError("username may only use letters, digits, '_', '.' and '-'");
    }
    return name;
}

const User& UserStore::addUser(std::string_view username, Role role, std::string_view password) {
    const std::string name = normalizeUsername(username);
    if (find(name) != nullptr) throw ValidationError("user '" + name + "' already exists");
    if (password.size() < kMinPasswordLength) {
        throw ValidationError("password must be at least " + std::to_string(kMinPasswordLength) +
                              " characters");
    }
    User user;
    user.username = name;
    user.role = role;
    user.saltHex = generateSalt();
    user.hashHex = hashPassword(user.saltHex, password);
    users_.push_back(std::move(user));
    return users_.back();
}

std::optional<User> UserStore::authenticate(std::string_view username,
                                            std::string_view password) const {
    const User* user = find(username);
    if (user == nullptr) {
        // Do the same amount of hashing work for unknown users.
        (void)hashPassword("00000000000000000000000000000000", password);
        return std::nullopt;
    }
    if (!constantTimeEquals(hashPassword(user->saltHex, password), user->hashHex)) {
        return std::nullopt;
    }
    return *user;
}

const User* UserStore::find(std::string_view username) const noexcept {
    const std::string wanted = text::toLower(text::trim(username));
    for (const User& u : users_) {
        if (u.username == wanted) return &u;
    }
    return nullptr;
}

UserStore UserStore::readCsv(std::istream& in) {
    UserStore store;
    std::string line;
    int lineNo = 0;
    bool headerSeen = false;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::string trimmed = text::trim(line);
        if (trimmed.empty()) continue;
        const std::string where = "line " + std::to_string(lineNo) + ": ";
        if (!headerSeen) {
            if (!text::iequals(trimmed, kHeader)) {
                throw ParseError(where + "expected header '" + kHeader + "'");
            }
            headerSeen = true;
            continue;
        }
        const auto f = text::split(trimmed, ',');
        if (f.size() != 4) throw ParseError(where + "expected 4 fields");
        User user;
        try {
            user.username = normalizeUsername(f[0]);
        } catch (const ValidationError& e) {
            throw ParseError(where + e.what());
        }
        const auto role = parseRole(f[1]);
        if (!role) throw ParseError(where + "unknown role '" + text::trim(f[1]) + "'");
        user.role = *role;
        user.saltHex = text::toLower(text::trim(f[2]));
        user.hashHex = text::toLower(text::trim(f[3]));
        if (!isHex(user.saltHex, 32)) throw ParseError(where + "salt must be 32 hex characters");
        if (!isHex(user.hashHex, 64)) throw ParseError(where + "hash must be 64 hex characters");
        if (store.find(user.username) != nullptr) {
            throw ParseError(where + "duplicate user '" + user.username + "'");
        }
        store.users_.push_back(std::move(user));
    }
    return store;
}

void UserStore::writeCsv(std::ostream& out) const {
    out << kHeader << '\n';
    for (const User& u : users_) {
        out << u.username << ',' << toString(u.role) << ',' << u.saltHex << ',' << u.hashHex
            << '\n';
    }
}

UserStore UserStore::loadCsv(const std::filesystem::path& file) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return UserStore{};
    std::ifstream in(file);
    if (!in) throw StorageError("cannot open " + file.string());
    try {
        return readCsv(in);
    } catch (const ParseError& e) {
        throw ParseError(file.string() + ", " + e.what());
    }
}

void UserStore::saveCsv(const std::filesystem::path& file) const {
    if (file.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
    }
    std::filesystem::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) throw StorageError("cannot write " + tmp.string());
        writeCsv(out);
        out.flush();
        if (!out) throw StorageError("failed while writing " + tmp.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmp, file, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw StorageError("cannot replace " + file.string());
    }
}

}  // namespace wms
