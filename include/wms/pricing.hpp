// pricing.hpp - order totals and the bulk discount rule.
//
// The 2021 program printed `price * quantity` as the amount to pay. The bulk
// discount below was added in the 2026 refactor:
//
//   quantity  1-19  ->  no discount
//   quantity 20-49  ->   5% off the line subtotal
//   quantity 50+    ->  10% off the line subtotal
//
// The discount is rounded half-up to the nearest cent.
#pragma once

#include <array>
#include <string>

#include "wms/money.hpp"
#include "wms/stock.hpp"

namespace wms {

struct DiscountTier {
    int minQuantity;
    int percent;
};

// Highest tier first; the first tier whose minimum is met applies.
inline constexpr std::array<DiscountTier, 2> kBulkDiscountTiers{{
    {50, 10},
    {20, 5},
}};

// Returns the discount percentage for a given order quantity (0, 5 or 10).
int discountPercentFor(int quantity) noexcept;

struct Receipt {
    std::string product;
    int quantity = 0;
    Cents unitPrice = 0;
    Cents subtotal = 0;
    int discountPercent = 0;
    Cents discount = 0;
    Cents total = 0;
};

// Prices an order for `quantity` units of `item` without changing stock.
// Throws ValidationError if quantity <= 0 and InsufficientStockError if the
// item does not have enough units.
Receipt priceOrder(const Stock& item, int quantity);

}  // namespace wms
