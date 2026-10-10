// Role-based access control: the matrix itself and its enforcement.
#include "mini_test.hpp"
#include "wms/errors.hpp"
#include "wms/permissions.hpp"
#include "wms/warehouse.hpp"

using namespace wms;

namespace {

User userWith(Role role) {
    User u;
    u.username = "test";
    u.role = role;
    return u;
}

Warehouse sampleWarehouse() {
    Inventory inv;
    inv.add(Stock("Pallet Jack", 34900, 12));
    inv.add(Stock("Packing Tape", 325, 600));
    return Warehouse(inv);
}

}  // namespace

TEST_CASE("Permissions", "Supplier may view, add, refill and remove but not purchase") {
    CHECK(isAllowed(Role::Supplier, Action::ViewStock));
    CHECK(isAllowed(Role::Supplier, Action::AddProduct));
    CHECK(isAllowed(Role::Supplier, Action::Refill));
    CHECK(isAllowed(Role::Supplier, Action::RemoveProduct));
    CHECK(!isAllowed(Role::Supplier, Action::Purchase));
}

TEST_CASE("Permissions", "Customer may view and purchase only") {
    CHECK(isAllowed(Role::Customer, Action::ViewStock));
    CHECK(isAllowed(Role::Customer, Action::Purchase));
    CHECK(!isAllowed(Role::Customer, Action::AddProduct));
    CHECK(!isAllowed(Role::Customer, Action::Refill));
    CHECK(!isAllowed(Role::Customer, Action::RemoveProduct));
}

TEST_CASE("Permissions", "Picker may view and refill only") {
    CHECK(isAllowed(Role::Picker, Action::ViewStock));
    CHECK(isAllowed(Role::Picker, Action::Refill));
    CHECK(!isAllowed(Role::Picker, Action::AddProduct));
    CHECK(!isAllowed(Role::Picker, Action::RemoveProduct));
    CHECK(!isAllowed(Role::Picker, Action::Purchase));
}

TEST_CASE("Permissions", "allowedActions lists actions in menu order") {
    const auto s = allowedActions(Role::Supplier);
    CHECK_EQ(s.size(), std::size_t{4});
    CHECK(s[0] == Action::ViewStock && s[3] == Action::RemoveProduct);
    const auto c = allowedActions(Role::Customer);
    CHECK(c.size() == 2 && c[1] == Action::Purchase);
    const auto p = allowedActions(Role::Picker);
    CHECK(p.size() == 2 && p[1] == Action::Refill);
}

TEST_CASE("Permissions", "requirePermission throws PermissionDeniedError") {
    CHECK_NOTHROW(requirePermission(Role::Customer, Action::Purchase));
    CHECK_THROWS_AS(requirePermission(Role::Customer, Action::RemoveProduct),
                    PermissionDeniedError);
}

TEST_CASE("Permissions", "parseRole accepts any letter case and rejects unknown roles") {
    CHECK(parseRole("supplier") == Role::Supplier);
    CHECK(parseRole(" CUSTOMER ") == Role::Customer);
    CHECK(parseRole("Picker") == Role::Picker);
    CHECK(!parseRole("admin").has_value());
}

TEST_CASE("Warehouse", "Supplier operations succeed end to end") {
    Warehouse w = sampleWarehouse();
    const User s = userWith(Role::Supplier);
    w.addProduct(s, "Hand Truck", 8900, 18);
    w.refill(s, "Pallet Jack", 3);
    w.removeProduct(s, "Packing Tape");
    CHECK_EQ(w.viewStock(s).size(), std::size_t{2});
    CHECK_EQ(w.inventory().get("Pallet Jack").quantity(), 15);
    CHECK_THROWS_AS(w.purchase(s, "Pallet Jack", 1), PermissionDeniedError);
}

TEST_CASE("Warehouse", "Customer is blocked from stock changes, inventory untouched") {
    Warehouse w = sampleWarehouse();
    const User c = userWith(Role::Customer);
    CHECK_THROWS_AS(w.addProduct(c, "Hand Truck", 8900, 18), PermissionDeniedError);
    CHECK_THROWS_AS(w.refill(c, "Pallet Jack", 3), PermissionDeniedError);
    CHECK_THROWS_AS(w.removeProduct(c, "Pallet Jack"), PermissionDeniedError);
    CHECK_EQ(w.inventory().size(), std::size_t{2});
    CHECK_EQ(w.inventory().get("Pallet Jack").quantity(), 12);
    const Receipt r = w.purchase(c, "Packing Tape", 60);
    CHECK_EQ(r.total, Cents{17550});
    CHECK_EQ(w.inventory().get("Packing Tape").quantity(), 540);
}

TEST_CASE("Warehouse", "Picker can refill but not add, remove or purchase") {
    Warehouse w = sampleWarehouse();
    const User p = userWith(Role::Picker);
    w.refill(p, "Packing Tape", 40);
    CHECK_EQ(w.inventory().get("Packing Tape").quantity(), 640);
    CHECK_THROWS_AS(w.addProduct(p, "Hand Truck", 8900, 18), PermissionDeniedError);
    CHECK_THROWS_AS(w.removeProduct(p, "Pallet Jack"), PermissionDeniedError);
    CHECK_THROWS_AS(w.quote(p, "Pallet Jack", 1), PermissionDeniedError);
    CHECK_THROWS_AS(w.purchase(p, "Pallet Jack", 1), PermissionDeniedError);
}

TEST_CASE("Warehouse", "permission is checked before input validation") {
    // A Customer asking to refill an unknown product learns nothing about it.
    Warehouse w = sampleWarehouse();
    CHECK_THROWS_AS(w.refill(userWith(Role::Customer), "Forklift", -5), PermissionDeniedError);
}

TEST_CASE("Warehouse", "quote prices an order without changing stock") {
    Warehouse w = sampleWarehouse();
    const Receipt q = w.quote(userWith(Role::Customer), "Pallet Jack", 2);
    CHECK_EQ(q.total, Cents{69800});
    CHECK_EQ(w.inventory().get("Pallet Jack").quantity(), 12);
}
