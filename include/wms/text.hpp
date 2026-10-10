// text.hpp - small string helpers shared by the parsers and the CLI.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wms::text {

// Removes leading and trailing spaces, tabs, CR and LF.
std::string trim(std::string_view s);

// ASCII lower-casing (product names and usernames are ASCII only).
std::string toLower(std::string_view s);

// Case-insensitive ASCII comparison.
bool iequals(std::string_view a, std::string_view b);

// Splits on a single delimiter character; keeps empty fields.
std::vector<std::string> split(std::string_view s, char delimiter);

// Parses a whole (trimmed) string as a base-10 int. Returns nullopt on any
// junk, overflow or empty input, so "12abc" and "" are both rejected.
std::optional<int> parseInt(std::string_view s);

}  // namespace wms::text
