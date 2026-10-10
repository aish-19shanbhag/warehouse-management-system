// SHA-256 against published FIPS 180-4 / NIST test vectors.
#include <string>

#include "mini_test.hpp"
#include "wms/sha256.hpp"

using namespace wms;

TEST_CASE("SHA-256", "empty string") {
    CHECK_EQ(Sha256::hexDigest(""),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

TEST_CASE("SHA-256", "\"abc\" (FIPS 180-4 one-block example)") {
    CHECK_EQ(Sha256::hexDigest("abc"),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST_CASE("SHA-256", "448-bit message (FIPS 180-4 two-block example)") {
    CHECK_EQ(Sha256::hexDigest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
             std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

TEST_CASE("SHA-256", "896-bit message") {
    CHECK_EQ(Sha256::hexDigest("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                               "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu"),
             std::string("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1"));
}

TEST_CASE("SHA-256", "one million 'a' characters, fed in uneven chunks") {
    Sha256 h;
    const std::string chunk(997, 'a');
    std::size_t remaining = 1000000;
    while (remaining > 0) {
        const std::size_t n = remaining < chunk.size() ? remaining : chunk.size();
        h.update(std::string_view(chunk).substr(0, n));
        remaining -= n;
    }
    const auto d = h.finish();
    CHECK_EQ(toHex(d.data(), d.size()),
             std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST_CASE("SHA-256", "padding boundaries at 55, 56 and 64 bytes match one-shot hashing") {
    // 55 bytes fits padding in one block; 56 forces a second block.
    for (std::size_t len : {55u, 56u, 63u, 64u, 65u}) {
        const std::string msg(len, 'x');
        Sha256 h;
        for (char c : msg) h.update(std::string_view(&c, 1));
        const auto d = h.finish();
        CHECK_EQ(toHex(d.data(), d.size()), Sha256::hexDigest(msg));
    }
    CHECK_EQ(Sha256::hexDigest(std::string(56, 'a')),
             std::string("b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"));
}
