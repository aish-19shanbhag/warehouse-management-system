// warehouse.hpp - the application service used by the CLI.
//
// Every public method takes the logged-in user, checks the permission matrix
// first, applies the change to a copy of the inventory, saves that copy, and
// only then makes it current. If the save fails, the in-memory state is left
// exactly as it was (strong exception guarantee).
#pragma once

#include <filesystem>
#include <string_view>

#include "wms/auth.hpp"
#include "wms/inventory.hpp"
#include "wms/pricing.hpp"

namespace wms {

class Warehouse {
public:
    // An empty path keeps everything in memory (used by the tests).
    explicit Warehouse(Inventory inventory, std::filesystem::path inventoryFile = {});

    static Warehouse open(const std::filesystem::path& inventoryFile);

    const Inventory& viewStock(const User& user) const;
    void addProduct(const User& user, std::string_view name, Cents unitPrice, int quantity);
    void refill(const User& user, std::string_view name, int quantity);
    void removeProduct(const User& user, std::string_view name);

    // quote() prices an order without changing anything; purchase() commits it.
    Receipt quote(const User& user, std::string_view name, int quantity) const;
    Receipt purchase(const User& user, std::string_view name, int quantity);

    // Unchecked read access for tests and diagnostics.
    const Inventory& inventory() const noexcept { return inventory_; }

private:
    void commit(Inventory next);

    Inventory inventory_;
    std::filesystem::path file_;
};

}  // namespace wms
