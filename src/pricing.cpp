#include "wms/pricing.hpp"

#include "wms/errors.hpp"

namespace wms {

int discountPercentFor(int quantity) noexcept {
    for (const DiscountTier& tier : kBulkDiscountTiers) {
        if (quantity >= tier.minQuantity) return tier.percent;
    }
    return 0;
}

Receipt priceOrder(const Stock& item, int quantity) {
    if (quantity <= 0) throw ValidationError("quantity must be at least 1");
    if (quantity > item.quantity()) {
        throw InsufficientStockError("only " + std::to_string(item.quantity()) + " unit(s) of '" +
                                     item.name() + "' in stock, " + std::to_string(quantity) +
                                     " requested");
    }

    Receipt r;
    r.product = item.name();
    r.quantity = quantity;
    r.unitPrice = item.unitPrice();
    r.subtotal = item.unitPrice() * static_cast<Cents>(quantity);
    r.discountPercent = discountPercentFor(quantity);
    r.discount = (r.subtotal * r.discountPercent + 50) / 100;  // round half-up to the cent
    r.total = r.subtotal - r.discount;
    return r;
}

}  // namespace wms
