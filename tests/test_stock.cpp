// Stock: construction, withdraw, refill, name rules, and money parsing.
#include "mini_test.hpp"
#include "wms/errors.hpp"
#include "wms/money.hpp"
#include "wms/stock.hpp"

using namespace wms;

TEST_CASE("Stock", "constructor stores name, price and quantity") {
    Stock s("Pallet Jack", 34900, 12);
    CHECK_EQ(s.name(), std::string("Pallet Jack"));
    CHECK_EQ(s.unitPrice(), Cents{34900});
    CHECK_EQ(s.quantity(), 12);
}

TEST_CASE("Stock", "constructor trims surrounding whitespace in the name") {
    Stock s("   Hand Truck  ", 8900, 1);
    CHECK_EQ(s.name(), std::string("Hand Truck"));
}

TEST_CASE("Stock", "zero starting quantity is allowed") {
    CHECK_NOTHROW(Stock("Empty Bin", 100, 0));
}

TEST_CASE("Stock", "rejects negative quantity") {
    CHECK_THROWS_AS(Stock("Box", 100, -1), ValidationError);
}

TEST_CASE("Stock", "rejects zero and negative prices") {
    CHECK_THROWS_AS(Stock("Box", 0, 5), ValidationError);
    CHECK_THROWS_AS(Stock("Box", -250, 5), ValidationError);
}

TEST_CASE("Stock", "rejects quantity above the per-item limit") {
    CHECK_THROWS_AS(Stock("Box", 100, Stock::kMaxQuantity + 1), ValidationError);
}

TEST_CASE("Stock", "withdraw reduces quantity") {
    Stock s("Tape", 325, 10);
    s.withdraw(4);
    CHECK_EQ(s.quantity(), 6);
    s.withdraw(6);
    CHECK_EQ(s.quantity(), 0);
}

TEST_CASE("Stock", "withdraw more than available throws and leaves quantity unchanged") {
    Stock s("Tape", 325, 3);
    CHECK_THROWS_AS(s.withdraw(4), InsufficientStockError);
    CHECK_EQ(s.quantity(), 3);
}

TEST_CASE("Stock", "withdraw rejects zero and negative quantities") {
    Stock s("Tape", 325, 3);
    CHECK_THROWS_AS(s.withdraw(0), ValidationError);
    CHECK_THROWS_AS(s.withdraw(-2), ValidationError);
    CHECK_EQ(s.quantity(), 3);
}

TEST_CASE("Stock", "refill increases quantity") {
    Stock s("Gloves", 650, 5);
    s.refill(20);
    CHECK_EQ(s.quantity(), 25);
}

TEST_CASE("Stock", "refill rejects non-positive quantities and overflow past the limit") {
    Stock s("Gloves", 650, Stock::kMaxQuantity - 1);
    CHECK_THROWS_AS(s.refill(0), ValidationError);
    CHECK_THROWS_AS(s.refill(-5), ValidationError);
    CHECK_THROWS_AS(s.refill(2), ValidationError);
    CHECK_EQ(s.quantity(), Stock::kMaxQuantity - 1);
    s.refill(1);
    CHECK_EQ(s.quantity(), Stock::kMaxQuantity);
}

TEST_CASE("Stock", "matches names case-insensitively") {
    Stock s("Barcode Scanner", 12999, 2);
    CHECK(s.matches("barcode scanner"));
    CHECK(s.matches("  BARCODE SCANNER "));
    CHECK(!s.matches("Barcode"));
}

TEST_CASE("Validation", "product names: empty, too long, commas, quotes, control chars") {
    CHECK_THROWS_AS(Stock::normalizeName("   "), ValidationError);
    CHECK_THROWS_AS(Stock::normalizeName(std::string(41, 'x')), ValidationError);
    CHECK_THROWS_AS(Stock::normalizeName("Box, Large"), ValidationError);
    CHECK_THROWS_AS(Stock::normalizeName("12\" Ruler"), ValidationError);
    CHECK_THROWS_AS(Stock::normalizeName("Tab\tName"), ValidationError);
    CHECK_EQ(Stock::normalizeName(std::string(40, 'x')), std::string(40, 'x'));
}

TEST_CASE("Validation", "the 2021 buffer overflow input (12+ chars) is handled safely") {
    // The legacy program read a 12-character password into char pass[10].
    // Long input is now just a std::string that gets validated.
    CHECK_NOTHROW(Stock("Suppliaccess-long-name", 100, 1));
    CHECK_THROWS_AS(Stock(std::string(500, 'A'), 100, 1), ValidationError);
}

TEST_CASE("Money", "parses whole, one-decimal and two-decimal prices") {
    CHECK_EQ(parseMoney("12"), Cents{1200});
    CHECK_EQ(parseMoney("12.5"), Cents{1250});
    CHECK_EQ(parseMoney(" 0.99 "), Cents{99});
    CHECK_EQ(parseMoney("349.00"), Cents{34900});
}

TEST_CASE("Validation", "malformed and negative prices are rejected") {
    CHECK_THROWS_AS(parseMoney(""), ValidationError);
    CHECK_THROWS_AS(parseMoney("-3.00"), ValidationError);
    CHECK_THROWS_AS(parseMoney("12.345"), ValidationError);
    CHECK_THROWS_AS(parseMoney("1,200.00"), ValidationError);
    CHECK_THROWS_AS(parseMoney("abc"), ValidationError);
    CHECK_THROWS_AS(parseMoney("12."), ValidationError);
    CHECK_THROWS_AS(parseMoney(".50"), ValidationError);
    CHECK_THROWS_AS(parseMoney("99999999999"), ValidationError);
}

TEST_CASE("Money", "formats cents with exactly two decimals") {
    CHECK_EQ(formatMoney(0), std::string("0.00"));
    CHECK_EQ(formatMoney(5), std::string("0.05"));
    CHECK_EQ(formatMoney(1250), std::string("12.50"));
    CHECK_EQ(formatMoney(-1950), std::string("-19.50"));
}
