// Payment totals and the bulk discount rule.
#include "mini_test.hpp"
#include "wms/errors.hpp"
#include "wms/pricing.hpp"

using namespace wms;

TEST_CASE("Pricing", "discount tiers: 0% below 20, 5% from 20, 10% from 50") {
    CHECK_EQ(discountPercentFor(1), 0);
    CHECK_EQ(discountPercentFor(19), 0);
    CHECK_EQ(discountPercentFor(20), 5);
    CHECK_EQ(discountPercentFor(49), 5);
    CHECK_EQ(discountPercentFor(50), 10);
    CHECK_EQ(discountPercentFor(5000), 10);
}

TEST_CASE("Pricing", "small order total is price times quantity with no discount") {
    const Stock s("Pallet Jack", 34900, 12);
    const Receipt r = priceOrder(s, 3);
    CHECK_EQ(r.subtotal, Cents{104700});
    CHECK_EQ(r.discountPercent, 0);
    CHECK_EQ(r.discount, Cents{0});
    CHECK_EQ(r.total, Cents{104700});
}

TEST_CASE("Pricing", "order of exactly 20 units gets 5% off") {
    const Stock s("Safety Gloves", 650, 300);
    const Receipt r = priceOrder(s, 20);  // 130.00 subtotal
    CHECK_EQ(r.subtotal, Cents{13000});
    CHECK_EQ(r.discount, Cents{650});
    CHECK_EQ(r.total, Cents{12350});
}

TEST_CASE("Pricing", "order of 60 units gets 10% off") {
    const Stock s("Packing Tape", 325, 600);
    const Receipt r = priceOrder(s, 60);  // 195.00 subtotal
    CHECK_EQ(r.discountPercent, 10);
    CHECK_EQ(r.discount, Cents{1950});
    CHECK_EQ(r.total, Cents{17550});
}

TEST_CASE("Pricing", "discount is rounded half-up to the nearest cent") {
    const Stock s("Widget", 1, 1000);  // 0.01 each
    CHECK_EQ(priceOrder(s, 30).discount, Cents{2});   // 5% of 0.30 = 0.015 -> 0.02
    CHECK_EQ(priceOrder(s, 29).discount, Cents{1});   // 5% of 0.29 = 0.0145 -> 0.01
    CHECK_EQ(priceOrder(s, 50).discount, Cents{5});   // 10% of 0.50 = 0.05 exactly
}

TEST_CASE("Pricing", "money math is exact (no float drift)") {
    const Stock s("Stretch Wrap Roll", 1875, 140);
    const Receipt r = priceOrder(s, 7);
    CHECK_EQ(r.total, Cents{13125});  // 7 x 18.75 = 131.25 exactly
}

TEST_CASE("Pricing", "pricing does not change stock and rejects bad quantities") {
    const Stock s("Hand Truck", 8900, 18);
    CHECK_THROWS_AS(priceOrder(s, 0), ValidationError);
    CHECK_THROWS_AS(priceOrder(s, -1), ValidationError);
    CHECK_THROWS_AS(priceOrder(s, 19), InsufficientStockError);
    (void)priceOrder(s, 18);
    CHECK_EQ(s.quantity(), 18);
}

TEST_CASE("Pricing", "large orders do not overflow") {
    const Stock s("Expensive", kMaxUnitPriceCents, Stock::kMaxQuantity);
    const Receipt r = priceOrder(s, Stock::kMaxQuantity);
    CHECK_EQ(r.subtotal, kMaxUnitPriceCents * Stock::kMaxQuantity);
    CHECK_EQ(r.total, r.subtotal - r.subtotal / 10);
}
