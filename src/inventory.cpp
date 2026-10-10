#include "wms/inventory.hpp"

#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <system_error>

#include "wms/errors.hpp"
#include "wms/text.hpp"

namespace wms {

namespace {
constexpr const char* kHeader = "name,price,quantity";
}

void Inventory::add(Stock item) {
    if (contains(item.name())) {
        throw ValidationError("product '" + item.name() +
                              "' already exists; use Refill to add more units");
    }
    items_.push_back(std::move(item));
}

void Inventory::remove(std::string_view name) {
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        if (it->matches(name)) {
            items_.erase(it);
            return;
        }
    }
    throw NotFoundError("product '" + text::trim(name) + "' not found");
}

void Inventory::refill(std::string_view name, int qty) {
    find(name).refill(qty);
}

Receipt Inventory::purchase(std::string_view name, int qty) {
    Stock& item = find(name);
    Receipt receipt = priceOrder(item, qty);  // validates before anything changes
    item.withdraw(qty);
    return receipt;
}

bool Inventory::contains(std::string_view name) const noexcept {
    for (const Stock& item : items_) {
        if (item.matches(name)) return true;
    }
    return false;
}

const Stock& Inventory::get(std::string_view name) const {
    for (const Stock& item : items_) {
        if (item.matches(name)) return item;
    }
    throw NotFoundError("product '" + text::trim(name) + "' not found");
}

Stock& Inventory::find(std::string_view name) {
    return const_cast<Stock&>(static_cast<const Inventory&>(*this).get(name));
}

Inventory Inventory::readCsv(std::istream& in) {
    Inventory inventory;
    std::string line;
    int lineNo = 0;
    bool headerSeen = false;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::string trimmed = text::trim(line);
        if (trimmed.empty()) continue;
        if (!headerSeen) {
            if (!text::iequals(trimmed, kHeader)) {
                throw ParseError("line " + std::to_string(lineNo) + ": expected header '" +
                                 kHeader + "'");
            }
            headerSeen = true;
            continue;
        }
        const auto fields = text::split(trimmed, ',');
        if (fields.size() != 3) {
            throw ParseError("line " + std::to_string(lineNo) + ": expected 3 fields, found " +
                             std::to_string(fields.size()));
        }
        try {
            const auto qty = text::parseInt(fields[2]);
            if (!qty) throw ValidationError("'" + text::trim(fields[2]) + "' is not a whole number");
            inventory.add(Stock(fields[0], parseMoney(fields[1]), *qty));
        } catch (const ValidationError& e) {
            throw ParseError("line " + std::to_string(lineNo) + ": " + e.what());
        }
    }
    return inventory;
}

void Inventory::writeCsv(std::ostream& out) const {
    out << kHeader << '\n';
    for (const Stock& item : items_) {
        out << item.name() << ',' << formatMoney(item.unitPrice()) << ',' << item.quantity()
            << '\n';
    }
}

Inventory Inventory::loadCsv(const std::filesystem::path& file) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return Inventory{};
    std::ifstream in(file);
    if (!in) throw StorageError("cannot open " + file.string());
    try {
        return readCsv(in);
    } catch (const ParseError& e) {
        throw ParseError(file.string() + ", " + e.what());
    }
}

void Inventory::saveCsv(const std::filesystem::path& file) const {
    if (file.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file.parent_path(), ec);
    }
    std::filesystem::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) throw StorageError("cannot write " + tmp.string());
        writeCsv(out);
        out.flush();
        if (!out) throw StorageError("failed while writing " + tmp.string());
    }
    std::error_code ec;
    std::filesystem::rename(tmp, file, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw StorageError("cannot replace " + file.string());
    }
}

}  // namespace wms
