// inventory.hpp - the collection of product lines, with CSV persistence.
//
// Replaces the 2021 approach of dumping raw `stock` objects into a binary
// `shop.dat` file. The CSV file is human-readable, diff-friendly and portable
// across compilers, platforms and struct layouts:
//
//   name,price,quantity
//   Pallet Jack,349.00,12
#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

#include "wms/pricing.hpp"
#include "wms/stock.hpp"

namespace wms {

class Inventory {
public:
    // Adds a new product line. Throws ValidationError if a product with the
    // same name (case-insensitive) already exists; use refill() instead.
    void add(Stock item);

    // Removes a product line. Throws NotFoundError if it does not exist.
    void remove(std::string_view name);

    // Adds units to an existing product.
    void refill(std::string_view name, int qty);

    // Prices the order, removes the units and returns the receipt. Nothing
    // changes if any check fails.
    Receipt purchase(std::string_view name, int qty);

    bool contains(std::string_view name) const noexcept;
    const Stock& get(std::string_view name) const;  // throws NotFoundError
    const std::vector<Stock>& items() const noexcept { return items_; }
    std::size_t size() const noexcept { return items_.size(); }
    bool empty() const noexcept { return items_.empty(); }

    // CSV serialisation. readCsv throws ParseError with the line number.
    static Inventory readCsv(std::istream& in);
    void writeCsv(std::ostream& out) const;

    // File helpers. loadCsv returns an empty inventory if the file does not
    // exist yet. saveCsv writes to a temporary file and then renames it over
    // the target, so a crash mid-write never leaves a half-written file.
    static Inventory loadCsv(const std::filesystem::path& file);
    void saveCsv(const std::filesystem::path& file) const;

private:
    Stock& find(std::string_view name);
    std::vector<Stock> items_;
};

}  // namespace wms
