// money.hpp - prices are stored as integer cents to avoid floating-point drift.
//
// The 2021 version stored prices as `float` and printed `pr * qty` directly,
// which can show totals such as 19.499998. Integer cents make every total exact.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wms {

using Cents = std::int64_t;

// Largest accepted unit price: 1,000,000.00
inline constexpr Cents kMaxUnitPriceCents = 100'000'000;

// Parses "12", "12.5" or "12.50" into cents. Rejects negatives, more than two
// decimal places, thousands separators and any other junk with ValidationError.
Cents parseMoney(std::string_view text);

// Formats cents as "12.50" (no currency symbol, always two decimals).
std::string formatMoney(Cents cents);

}  // namespace wms
