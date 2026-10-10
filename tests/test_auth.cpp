// Authentication: salted hashing, login checks, users.csv parsing.
#include <filesystem>
#include <sstream>

#include "mini_test.hpp"
#include "wms/auth.hpp"
#include "wms/errors.hpp"
#include "wms/sha256.hpp"

using namespace wms;
namespace fs = std::filesystem;

TEST_CASE("Auth", "hashPassword is SHA-256 of salt followed by password") {
    const std::string salt = "00112233445566778899aabbccddeeff";
    CHECK_EQ(hashPassword(salt, "secret123"), Sha256::hexDigest(salt + "secret123"));
}

TEST_CASE("Auth", "salts are 32 hex chars and differ between calls") {
    const std::string a = generateSalt();
    const std::string b = generateSalt();
    CHECK_EQ(a.size(), std::size_t{32});
    CHECK(a != b);
}

TEST_CASE("Auth", "same password gives different hashes for different users") {
    UserStore store;
    const User& u1 = store.addUser("alice", Role::Customer, "SamePass99");
    const std::string h1 = u1.hashHex;
    const User& u2 = store.addUser("bob", Role::Customer, "SamePass99");
    CHECK(h1 != u2.hashHex);
}

TEST_CASE("Auth", "correct password authenticates and returns the role") {
    UserStore store;
    store.addUser("supplier", Role::Supplier, "Supplier@2026");
    const auto user = store.authenticate("supplier", "Supplier@2026");
    CHECK(user.has_value());
    CHECK(user->role == Role::Supplier);
}

TEST_CASE("Auth", "usernames are case-insensitive, passwords are case-sensitive") {
    UserStore store;
    store.addUser("Picker", Role::Picker, "Picker@2026");
    CHECK(store.authenticate("PICKER", "Picker@2026").has_value());
    CHECK(!store.authenticate("picker", "picker@2026").has_value());
}

TEST_CASE("Auth", "wrong password, unknown user and empty input are all rejected") {
    UserStore store;
    store.addUser("customer", Role::Customer, "Customer@2026");
    CHECK(!store.authenticate("customer", "Customer@2025").has_value());
    CHECK(!store.authenticate("nobody", "Customer@2026").has_value());
    CHECK(!store.authenticate("", "").has_value());
    // The hard-coded 2021 passwords no longer grant anything.
    CHECK(!store.authenticate("customer", "Customaccess").has_value());
}

TEST_CASE("Auth", "plaintext passwords are never stored") {
    UserStore store;
    store.addUser("supplier", Role::Supplier, "Supplier@2026");
    std::ostringstream out;
    store.writeCsv(out);
    CHECK(out.str().find("Supplier@2026") == std::string::npos);
}

TEST_CASE("Validation", "addUser rejects bad usernames, duplicates and short passwords") {
    UserStore store;
    store.addUser("supplier", Role::Supplier, "Supplier@2026");
    CHECK_THROWS_AS(store.addUser("SUPPLIER", Role::Picker, "Another@2026"), ValidationError);
    CHECK_THROWS_AS(store.addUser("ab", Role::Picker, "LongEnough1"), ValidationError);
    CHECK_THROWS_AS(store.addUser("bad name", Role::Picker, "LongEnough1"), ValidationError);
    CHECK_THROWS_AS(store.addUser("eve,admin", Role::Picker, "LongEnough1"), ValidationError);
    CHECK_THROWS_AS(store.addUser("shorty", Role::Picker, "short"), ValidationError);
    CHECK_EQ(store.users().size(), std::size_t{1});
}

TEST_CASE("Auth", "constantTimeEquals compares exactly") {
    CHECK(constantTimeEquals("abc", "abc"));
    CHECK(!constantTimeEquals("abc", "abd"));
    CHECK(!constantTimeEquals("abc", "abcd"));
    CHECK(constantTimeEquals("", ""));
}

TEST_CASE("Persistence", "users.csv round trip keeps logins working") {
    UserStore store;
    store.addUser("supplier", Role::Supplier, "Supplier@2026");
    store.addUser("picker", Role::Picker, "Picker@2026");
    std::stringstream buf;
    store.writeCsv(buf);
    const UserStore loaded = UserStore::readCsv(buf);
    CHECK_EQ(loaded.users().size(), std::size_t{2});
    CHECK(loaded.authenticate("picker", "Picker@2026").has_value());
    CHECK(loaded.find("picker")->role == Role::Picker);
}

TEST_CASE("Validation", "corrupt users.csv rows are rejected") {
    const std::string header = "username,role,salt,sha256\n";
    const std::string salt(32, 'a');
    const std::string hash(64, 'b');
    const std::string bad[] = {
        "user,role\n",
        header + "alice,Admin," + salt + "," + hash + "\n",
        header + "alice,Customer,xyz," + hash + "\n",
        header + "alice,Customer," + salt + ",1234\n",
        header + "alice,Customer," + salt + "," + hash + "\nALICE,Picker," + salt + "," + hash +
            "\n",
    };
    for (const std::string& text : bad) {
        std::istringstream in(text);
        CHECK_THROWS_AS(UserStore::readCsv(in), ParseError);
    }
}

TEST_CASE("Auth", "shipped data/users.csv holds the three documented demo accounts") {
    const UserStore store = UserStore::loadCsv(fs::path(WMS_SOURCE_DIR) / "data" / "users.csv");
    const auto s = store.authenticate("supplier", "Supplier@2026");
    const auto c = store.authenticate("customer", "Customer@2026");
    const auto p = store.authenticate("picker", "Picker@2026");
    CHECK(s.has_value() && s->role == Role::Supplier);
    CHECK(c.has_value() && c->role == Role::Customer);
    CHECK(p.has_value() && p->role == Role::Picker);
}
