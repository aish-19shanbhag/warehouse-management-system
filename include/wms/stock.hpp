// stock.hpp - one product line held in the warehouse.
//
// `Stock` is the modern version of the 2021 `stock` class: it owns a product's
// name, unit price and quantity, and the per-item operations that change them
// (withdraw and refill). Every operation validates its input and keeps the
// object in a valid state: quantity never goes negative and price is always
// positive. Adding and removing whole product lines is done by `Inventory`.
#pragma once

#include <string>
#include <string_view>

#include "wms/money.hpp"

namespace wms {

class Stock {
public:
    static constexpr int kMaxQuantity = 1'000'000;
    static constexpr std::size_t kMaxNameLength = 40;

    // Throws ValidationError for an invalid name, a non-positive price or a
    // quantity outside [0, kMaxQuantity].
    Stock(std::string_view name, Cents unitPrice, int quantity);

    const std::string& name() const noexcept { return name_; }
    Cents unitPrice() const noexcept { return unitPrice_; }
    int quantity() const noexcept { return quantity_; }

    // Removes `qty` units. Throws ValidationError if qty <= 0 and
    // InsufficientStockError if qty exceeds what is on hand (nothing changes).
    void withdraw(int qty);

    // Adds `qty` units. Throws ValidationError if qty <= 0 or if the result
    // would exceed kMaxQuantity.
    void refill(int qty);

    // Case-insensitive name comparison used for lookups.
    bool matches(std::string_view productName) const;

    // Trims a raw name and checks it: 1-40 printable ASCII characters, no
    // commas or double quotes (they would break the CSV file).
    static std::string normalizeName(std::string_view raw);

private:
    std::string name_;
    Cents unitPrice_;
    int quantity_;
};

}  // namespace wms
