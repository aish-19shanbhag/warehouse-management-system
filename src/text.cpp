#include "wms/text.hpp"

#include <cctype>
#include <charconv>

namespace wms::text {

namespace {
bool isSpace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
char lower(char c) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}
}  // namespace

std::string trim(std::string_view s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && isSpace(s[begin])) ++begin;
    while (end > begin && isSpace(s[end - 1])) --end;
    return std::string(s.substr(begin, end - begin));
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) c = lower(c);
    return out;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

std::vector<std::string> split(std::string_view s, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = s.find(delimiter, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(s.substr(start));
            break;
        }
        parts.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

std::optional<int> parseInt(std::string_view s) {
    const std::string t = trim(s);
    if (t.empty()) return std::nullopt;
    const char* first = t.data();
    const char* last = t.data() + t.size();
    if (*first == '+') ++first;  // from_chars does not accept a leading '+'
    int value = 0;
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc() || ptr != last || first == last) return std::nullopt;
    return value;
}

}  // namespace wms::text
