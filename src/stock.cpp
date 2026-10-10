#include "wms/stock.hpp"

#include "wms/errors.hpp"
#include "wms/text.hpp"

namespace wms {

std::string Stock::normalizeName(std::string_view raw) {
    std::string name = text::trim(raw);
    if (name.empty()) throw ValidationError("product name cannot be empty");
    if (name.size() > kMaxNameLength) {
        throw ValidationError("product name must be at most " + std::to_string(kMaxNameLength) +
                              " characters");
    }
    for (char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u > 0x7E) {
            throw ValidationError("product name may only contain printable ASCII characters");
        }
        if (c == ',' || c == '"') {
            throw ValidationError("product name cannot contain commas or double quotes");
        }
    }
    return name;
}

Stock::Stock(std::string_view name, Cents unitPrice, int quantity)
    : name_(normalizeName(name)), unitPrice_(unitPrice), quantity_(quantity) {
    if (unitPrice_ <= 0) throw ValidationError("price must be greater than zero");
    if (unitPrice_ > kMaxUnitPriceCents) throw ValidationError("price is too large");
    if (quantity_ < 0) throw ValidationError("quantity cannot be negative");
    if (quantity_ > kMaxQuantity) {
        throw ValidationError("quantity cannot exceed " + std::to_string(kMaxQuantity));
    }
}

void Stock::withdraw(int qty) {
    if (qty <= 0) throw ValidationError("quantity must be at least 1");
    if (qty > quantity_) {
        throw InsufficientStockError("only " + std::to_string(quantity_) + " unit(s) of '" + name_ +
                                     "' in stock, " + std::to_string(qty) + " requested");
    }
    quantity_ -= qty;
}

void Stock::refill(int qty) {
    if (qty <= 0) throw ValidationError("quantity must be at least 1");
    if (qty > kMaxQuantity - quantity_) {
        throw ValidationError("refill would exceed the limit of " + std::to_string(kMaxQuantity) +
                              " units for '" + name_ + "'");
    }
    quantity_ += qty;
}

bool Stock::matches(std::string_view productName) const {
    return text::iequals(name_, text::trim(productName));
}

}  // namespace wms
