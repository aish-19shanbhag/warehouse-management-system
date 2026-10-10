// End-to-end CLI sessions driven through string streams, plus Warehouse
// persistence (every committed change reaches the CSV file).
#include <atomic>
#include <chrono>
#include <filesystem>
#include <sstream>

#include "mini_test.hpp"
#include "wms/cli.hpp"
#include "wms/errors.hpp"

using namespace wms;
namespace fs = std::filesystem;

namespace {

UserStore demoUsers() {
    UserStore users;
    users.addUser("supplier", Role::Supplier, "Supplier@2026");
    users.addUser("customer", Role::Customer, "Customer@2026");
    users.addUser("picker", Role::Picker, "Picker@2026");
    return users;
}

Inventory demoInventory() {
    Inventory inv;
    inv.add(Stock("Pallet Jack", 34900, 12));
    inv.add(Stock("Packing Tape", 325, 600));
    return inv;
}

std::optional<std::string> plainPassword(std::istream& in) {
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;
    return line;
}

// Runs a whole scripted session and returns everything the CLI printed.
std::string runSession(Warehouse& w, const std::string& script) {
    const UserStore users = demoUsers();
    std::istringstream in(script);
    std::ostringstream out;
    Cli cli(w, users, in, out, plainPassword, Cli::Options{});
    CHECK_EQ(cli.run(), 0);
    return out.str();
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("CLI", "customer purchase with bulk discount, end to end") {
    Warehouse w(demoInventory());
    const std::string out = runSession(
        w, "1\ncustomer\nCustomer@2026\n2\nPacking Tape\n60\ny\n3\n2\n");
    CHECK(contains(out, "Welcome, customer (Customer)"));
    CHECK(contains(out, "Bulk discount  -19.50 (10%)"));
    CHECK(contains(out, "Total due      175.50"));
    CHECK(contains(out, "540 unit(s) of 'Packing Tape' left"));
    CHECK_EQ(w.inventory().get("Packing Tape").quantity(), 540);
}

TEST_CASE("CLI", "declining the order summary leaves stock unchanged") {
    Warehouse w(demoInventory());
    const std::string out =
        runSession(w, "1\ncustomer\nCustomer@2026\n2\nPallet Jack\n2\nn\n3\n2\n");
    CHECK(contains(out, "Purchase cancelled"));
    CHECK_EQ(w.inventory().get("Pallet Jack").quantity(), 12);
}

TEST_CASE("CLI", "each role sees only its permitted menu entries") {
    Warehouse w(demoInventory());
    const std::string customer = runSession(w, "1\ncustomer\nCustomer@2026\n3\n2\n");
    CHECK(contains(customer, "CUSTOMER MENU"));
    CHECK(contains(customer, "2. Purchase"));
    CHECK(!contains(customer, "Refill stock"));
    CHECK(!contains(customer, "Remove product"));

    const std::string picker = runSession(w, "1\npicker\nPicker@2026\n3\n2\n");
    CHECK(contains(picker, "2. Refill stock"));
    CHECK(!contains(picker, "Purchase"));
    CHECK(!contains(picker, "Add new product"));

    const std::string supplier = runSession(w, "1\nsupplier\nSupplier@2026\n5\n2\n");
    CHECK(contains(supplier, "4. Remove product"));
    CHECK(!contains(supplier, "Purchase"));
}

TEST_CASE("CLI", "three failed logins return to the main menu") {
    Warehouse w(demoInventory());
    const std::string out = runSession(w, "1\ncustomer\nx\ncustomer\ny\ncustomer\nz\n2\n");
    CHECK(contains(out, "2 attempt(s) left"));
    CHECK(contains(out, "Too many failed attempts"));
    CHECK(!contains(out, "Welcome"));
}

TEST_CASE("CLI", "supplier add, refill and remove through the menus") {
    Warehouse w(demoInventory());
    const std::string out = runSession(w,
                                       "1\nsupplier\nSupplier@2026\n"
                                       "2\nHand Truck\n89.00\n18\n"
                                       "3\nhand truck\n2\n"
                                       "4\nPallet Jack\ny\n"
                                       "5\n2\n");
    CHECK(contains(out, "Added 'Hand Truck'"));
    CHECK(contains(out, "'Hand Truck' now has 20 unit(s)"));
    CHECK(contains(out, "Removed 'Pallet Jack'"));
    CHECK(!w.inventory().contains("Pallet Jack"));
}

TEST_CASE("CLI", "invalid input shows an error and the session continues") {
    Warehouse w(demoInventory());
    const std::string out = runSession(w,
                                       "1\nsupplier\nSupplier@2026\n"
                                       "2\nBox\n-4.00\n"         // negative price
                                       "3\nForklift\n5\n"        // unknown product
                                       "3\nPallet Jack\nabc\n-3\n"  // not a number, then negative
                                       "9\n"                     // out-of-range menu choice
                                       "5\n2\n");
    CHECK(contains(out, "Error: price cannot be negative"));
    CHECK(contains(out, "Error: product 'Forklift' not found"));
    CHECK(contains(out, "Please enter a whole number."));
    CHECK(contains(out, "Error: quantity must be at least 1"));
    CHECK(contains(out, "Please choose a number from 1 to 5."));
    CHECK(contains(out, "Logged out."));
    CHECK_EQ(w.inventory().get("Pallet Jack").quantity(), 12);
}

TEST_CASE("CLI", "end of input exits cleanly from any menu") {
    Warehouse w(demoInventory());
    const std::string out = runSession(w, "1\ncustomer\nCustomer@2026\n2\nPacking");
    CHECK(contains(out, "Goodbye."));
}

TEST_CASE("Persistence", "Warehouse writes every committed change to its CSV file") {
    static std::atomic<int> counter{0};
    const fs::path dir = fs::temp_directory_path() /
                         ("wms_cli_" +
                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                          "_" + std::to_string(counter++));
    fs::create_directories(dir);
    const fs::path file = dir / "inventory.csv";
    demoInventory().saveCsv(file);
    {
        Warehouse w = Warehouse::open(file);
        runSession(w, "1\ncustomer\nCustomer@2026\n2\nPacking Tape\n25\ny\n3\n2\n");
    }
    const Inventory reloaded = Inventory::loadCsv(file);
    CHECK_EQ(reloaded.get("Packing Tape").quantity(), 575);
    std::error_code ec;
    fs::remove_all(dir, ec);
}
