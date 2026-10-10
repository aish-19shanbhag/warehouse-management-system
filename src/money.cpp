#include "wms/money.hpp"

#include <cctype>

#include "wms/errors.hpp"
#include "wms/text.hpp"

namespace wms {

namespace {
bool allDigits(std::string_view s) noexcept {
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}
}  // namespace

Cents parseMoney(std::string_view text) {
    const std::string s = text::trim(text);
    if (s.empty()) throw ValidationError("price is empty");
    if (s.front() == '-') throw ValidationError("price cannot be negative");

    const std::size_t dot = s.find('.');
    const std::string whole = s.substr(0, dot);
    const std::string frac = (dot == std::string::npos) ? std::string() : s.substr(dot + 1);

    const bool wellFormed = !whole.empty() && allDigits(whole) && allDigits(frac) &&
                            frac.size() <= 2 && !(dot != std::string::npos && frac.empty());
    if (!wellFormed) {
        throw ValidationError("'" + s + "' is not a valid price (use a format like 12.50)");
    }
    if (whole.size() > 9) throw ValidationError("price is too large");

    Cents cents = std::stoll(whole) * 100;
    if (frac.size() == 1) cents += (frac[0] - '0') * 10;
    if (frac.size() == 2) cents += (frac[0] - '0') * 10 + (frac[1] - '0');
    if (cents > kMaxUnitPriceCents) throw ValidationError("price is too large");
    return cents;
}

std::string formatMoney(Cents cents) {
    const bool negative = cents < 0;
    const Cents abs = negative ? -cents : cents;
    std::string fraction = std::to_string(abs % 100);
    if (fraction.size() == 1) fraction.insert(fraction.begin(), '0');
    return (negative ? "-" : "") + std::to_string(abs / 100) + "." + fraction;
}

}  // namespace wms
