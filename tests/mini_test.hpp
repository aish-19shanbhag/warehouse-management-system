// mini_test.hpp - a tiny, self-contained unit test framework (no downloads).
//
//   TEST_CASE("Suite", "does something") { CHECK(x == 1); CHECK_EQ(a, b); }
//
// A failing check stops the current test case and is reported with its file
// and line. The runner prints one line per test and a summary, and returns a
// non-zero exit code if anything failed, which is what CTest and CI look at.
// Pass a substring as the first argument to run only matching tests.
#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mini {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

inline long& assertionCount() {
    static long count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* suite, const char* name, void (*body)()) {
        registry().push_back({suite, name, body});
    }
};

struct Failure {
    std::string message;
};

[[noreturn]] inline void fail(const char* file, int line, const std::string& message) {
    std::ostringstream os;
    os << file << ":" << line << ": " << message;
    throw Failure{os.str()};
}

template <typename T, typename = void>
struct IsStreamable : std::false_type {};
template <typename T>
struct IsStreamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T>
std::string show(const T& value) {
    if constexpr (IsStreamable<T>::value) {
        std::ostringstream os;
        os << value;
        return os.str();
    } else {
        return "<value>";
    }
}

inline int runAll(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int passed = 0;
    int failed = 0;
    std::vector<std::string> failures;

    for (const TestCase& t : registry()) {
        const std::string label = t.suite + " / " + t.name;
        if (!filter.empty() && label.find(filter) == std::string::npos) continue;
        std::string error;
        try {
            t.body();
        } catch (const Failure& f) {
            error = f.message;
        } catch (const std::exception& e) {
            error = std::string("unexpected exception: ") + e.what();
        } catch (...) {
            error = "unexpected non-standard exception";
        }
        if (error.empty()) {
            ++passed;
            std::cout << "[ PASS ] " << label << "\n";
        } else {
            ++failed;
            std::cout << "[ FAIL ] " << label << "\n         " << error << "\n";
            failures.push_back(label);
        }
    }

    std::cout << "\n" << (passed + failed) << " tests, " << passed << " passed, " << failed
              << " failed, " << assertionCount() << " assertions\n";
    for (const std::string& f : failures) std::cout << "  failed: " << f << "\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace mini

#define MINI_CAT2(a, b) a##b
#define MINI_CAT(a, b) MINI_CAT2(a, b)

#define TEST_CASE(suite, name)                                                       \
    static void MINI_CAT(mini_test_, __LINE__)();                                    \
    static const mini::Registrar MINI_CAT(mini_reg_, __LINE__){suite, name,          \
                                                               &MINI_CAT(mini_test_, __LINE__)}; \
    static void MINI_CAT(mini_test_, __LINE__)()

#define CHECK(expr)                                                         \
    do {                                                                    \
        ++mini::assertionCount();                                           \
        if (!(expr)) mini::fail(__FILE__, __LINE__, "CHECK(" #expr ") failed"); \
    } while (0)

#define CHECK_EQ(actual, expected)                                                        \
    do {                                                                                  \
        ++mini::assertionCount();                                                         \
        const auto& mini_a = (actual);                                                    \
        const auto& mini_e = (expected);                                                  \
        if (!(mini_a == mini_e))                                                          \
            mini::fail(__FILE__, __LINE__,                                                \
                       "CHECK_EQ(" #actual ", " #expected "): got " + mini::show(mini_a) + \
                           ", expected " + mini::show(mini_e));                           \
    } while (0)

#define CHECK_THROWS_AS(expr, ExceptionType)                                              \
    do {                                                                                  \
        ++mini::assertionCount();                                                         \
        bool mini_caught = false;                                                         \
        try {                                                                             \
            (void)(expr);                                                                 \
        } catch (const ExceptionType&) {                                                  \
            mini_caught = true;                                                           \
        } catch (...) {                                                                   \
            mini::fail(__FILE__, __LINE__, #expr " threw the wrong exception type");      \
        }                                                                                 \
        if (!mini_caught)                                                                 \
            mini::fail(__FILE__, __LINE__, #expr " did not throw " #ExceptionType);       \
    } while (0)

#define CHECK_NOTHROW(expr)                                                     \
    do {                                                                        \
        ++mini::assertionCount();                                               \
        try {                                                                   \
            (void)(expr);                                                       \
        } catch (...) {                                                         \
            mini::fail(__FILE__, __LINE__, #expr " threw unexpectedly");        \
        }                                                                       \
    } while (0)
