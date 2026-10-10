// Inventory: add/remove/refill/purchase and CSV persistence.
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "mini_test.hpp"
#include "wms/errors.hpp"
#include "wms/inventory.hpp"
#include "wms/text.hpp"

using namespace wms;
namespace fs = std::filesystem;

namespace {

Inventory sample() {
    Inventory inv;
    inv.add(Stock("Pallet Jack", 34900, 12));
    inv.add(Stock("Packing Tape", 325, 600));
    inv.add(Stock("Hand Truck", 8900, 18));
    return inv;
}

// A unique scratch directory under the system temp folder, removed on exit.
struct TempDir {
    fs::path path;
    TempDir() {
        static std::atomic<int> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() /
               ("wms_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

}  // namespace

TEST_CASE("Inventory", "add keeps insertion order and size") {
    Inventory inv = sample();
    CHECK_EQ(inv.size(), std::size_t{3});
    CHECK_EQ(inv.items()[0].name(), std::string("Pallet Jack"));
    CHECK_EQ(inv.items()[2].name(), std::string("Hand Truck"));
}

TEST_CASE("Inventory", "add rejects a duplicate product name (case-insensitive)") {
    Inventory inv = sample();
    CHECK_THROWS_AS(inv.add(Stock("packing tape", 100, 1)), ValidationError);
    CHECK_EQ(inv.size(), std::size_t{3});
}

TEST_CASE("Inventory", "get and contains find products case-insensitively") {
    Inventory inv = sample();
    CHECK(inv.contains("HAND TRUCK"));
    CHECK_EQ(inv.get("pallet jack").quantity(), 12);
    CHECK(!inv.contains("Forklift"));
}

TEST_CASE("Inventory", "remove deletes only the named product") {
    Inventory inv = sample();
    inv.remove("Packing Tape");
    CHECK_EQ(inv.size(), std::size_t{2});
    CHECK(!inv.contains("Packing Tape"));
    CHECK(inv.contains("Pallet Jack"));
}

TEST_CASE("Validation", "unknown products raise NotFoundError for every operation") {
    Inventory inv = sample();
    CHECK_THROWS_AS(inv.get("Forklift"), NotFoundError);
    CHECK_THROWS_AS(inv.remove("Forklift"), NotFoundError);
    CHECK_THROWS_AS(inv.refill("Forklift", 5), NotFoundError);
    CHECK_THROWS_AS(inv.purchase("Forklift", 1), NotFoundError);
}

TEST_CASE("Inventory", "refill adds units to an existing product") {
    Inventory inv = sample();
    inv.refill("hand truck", 7);
    CHECK_EQ(inv.get("Hand Truck").quantity(), 25);
}

TEST_CASE("Inventory", "purchase withdraws stock and returns a receipt") {
    Inventory inv = sample();
    const Receipt r = inv.purchase("Pallet Jack", 2);
    CHECK_EQ(r.product, std::string("Pallet Jack"));
    CHECK_EQ(r.total, Cents{69800});
    CHECK_EQ(inv.get("Pallet Jack").quantity(), 10);
}

TEST_CASE("Inventory", "failed purchase leaves stock unchanged") {
    Inventory inv = sample();
    CHECK_THROWS_AS(inv.purchase("Pallet Jack", 13), InsufficientStockError);
    CHECK_THROWS_AS(inv.purchase("Pallet Jack", 0), ValidationError);
    CHECK_EQ(inv.get("Pallet Jack").quantity(), 12);
}

TEST_CASE("Persistence", "CSV write produces the documented human-readable format") {
    std::ostringstream out;
    sample().writeCsv(out);
    CHECK_EQ(out.str(), std::string("name,price,quantity\n"
                                    "Pallet Jack,349.00,12\n"
                                    "Packing Tape,3.25,600\n"
                                    "Hand Truck,89.00,18\n"));
}

TEST_CASE("Persistence", "CSV round trip through a stream preserves every field") {
    std::stringstream buf;
    const Inventory original = sample();
    original.writeCsv(buf);
    const Inventory loaded = Inventory::readCsv(buf);
    CHECK_EQ(loaded.size(), original.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        CHECK_EQ(loaded.items()[i].name(), original.items()[i].name());
        CHECK_EQ(loaded.items()[i].unitPrice(), original.items()[i].unitPrice());
        CHECK_EQ(loaded.items()[i].quantity(), original.items()[i].quantity());
    }
}

TEST_CASE("Persistence", "save then load from disk round-trips and leaves no temp file") {
    TempDir dir;
    const fs::path file = dir.path / "inventory.csv";
    Inventory inv = sample();
    inv.purchase("Packing Tape", 50);
    inv.saveCsv(file);
    CHECK(fs::exists(file));
    CHECK(!fs::exists(dir.path / "inventory.csv.tmp"));

    const Inventory loaded = Inventory::loadCsv(file);
    CHECK_EQ(loaded.size(), std::size_t{3});
    CHECK_EQ(loaded.get("Packing Tape").quantity(), 550);
    CHECK_EQ(loaded.get("Pallet Jack").unitPrice(), Cents{34900});
}

TEST_CASE("Persistence", "loading a missing file gives an empty inventory") {
    TempDir dir;
    const Inventory inv = Inventory::loadCsv(dir.path / "does_not_exist.csv");
    CHECK(inv.empty());
}

TEST_CASE("Persistence", "reader tolerates CRLF line endings and blank lines") {
    std::istringstream in("name,price,quantity\r\n\r\nTape,3.25,10\r\n\r\n");
    const Inventory inv = Inventory::readCsv(in);
    CHECK_EQ(inv.size(), std::size_t{1});
    CHECK_EQ(inv.get("Tape").quantity(), 10);
}

TEST_CASE("Validation", "corrupt CSV rows are rejected with the line number") {
    const char* bad[] = {
        "wrong,header\nTape,3.25,10\n",
        "name,price,quantity\nTape,3.25\n",
        "name,price,quantity\nTape,-3.25,10\n",
        "name,price,quantity\nTape,3.25,-10\n",
        "name,price,quantity\nTape,3.25,ten\n",
        "name,price,quantity\nTape,3.25,10\ntape,1.00,1\n",
    };
    for (const char* text : bad) {
        std::istringstream in(text);
        CHECK_THROWS_AS(Inventory::readCsv(in), ParseError);
    }
    std::istringstream in("name,price,quantity\nTape,3.25,10\nBox,abc,1\n");
    try {
        Inventory::readCsv(in);
        CHECK(false);
    } catch (const ParseError& e) {
        CHECK(std::string(e.what()).find("line 3") != std::string::npos);
    }
}

TEST_CASE("Persistence", "the shipped data/inventory.csv sample loads cleanly") {
    const fs::path file = fs::path(WMS_SOURCE_DIR) / "data" / "inventory.csv";
    CHECK(fs::exists(file));
    const Inventory inv = Inventory::loadCsv(file);
    CHECK(inv.size() >= 5);
}

TEST_CASE("Validation", "parseInt accepts only whole numbers") {
    CHECK_EQ(text::parseInt(" 42 ").value_or(-1), 42);
    CHECK_EQ(text::parseInt("+7").value_or(-1), 7);
    CHECK(!text::parseInt("").has_value());
    CHECK(!text::parseInt("4.5").has_value());
    CHECK(!text::parseInt("12abc").has_value());
    CHECK(!text::parseInt("99999999999").has_value());
}
