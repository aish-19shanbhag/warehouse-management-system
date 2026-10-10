#include "wms/warehouse.hpp"

#include <utility>

#include "wms/permissions.hpp"

namespace wms {

Warehouse::Warehouse(Inventory inventory, std::filesystem::path inventoryFile)
    : inventory_(std::move(inventory)), file_(std::move(inventoryFile)) {}

Warehouse Warehouse::open(const std::filesystem::path& inventoryFile) {
    return Warehouse(Inventory::loadCsv(inventoryFile), inventoryFile);
}

const Inventory& Warehouse::viewStock(const User& user) const {
    requirePermission(user.role, Action::ViewStock);
    return inventory_;
}

void Warehouse::addProduct(const User& user, std::string_view name, Cents unitPrice,
                           int quantity) {
    requirePermission(user.role, Action::AddProduct);
    Inventory next = inventory_;
    next.add(Stock(name, unitPrice, quantity));
    commit(std::move(next));
}

void Warehouse::refill(const User& user, std::string_view name, int quantity) {
    requirePermission(user.role, Action::Refill);
    Inventory next = inventory_;
    next.refill(name, quantity);
    commit(std::move(next));
}

void Warehouse::removeProduct(const User& user, std::string_view name) {
    requirePermission(user.role, Action::RemoveProduct);
    Inventory next = inventory_;
    next.remove(name);
    commit(std::move(next));
}

Receipt Warehouse::quote(const User& user, std::string_view name, int quantity) const {
    requirePermission(user.role, Action::Purchase);
    return priceOrder(inventory_.get(name), quantity);
}

Receipt Warehouse::purchase(const User& user, std::string_view name, int quantity) {
    requirePermission(user.role, Action::Purchase);
    Inventory next = inventory_;
    Receipt receipt = next.purchase(name, quantity);
    commit(std::move(next));
    return receipt;
}

void Warehouse::commit(Inventory next) {
    if (!file_.empty()) next.saveCsv(file_);  // throws before anything changes in memory
    inventory_ = std::move(next);
}

}  // namespace wms
