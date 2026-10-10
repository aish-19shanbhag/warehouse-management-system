#include "wms/cli.hpp"

#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

#include "wms/errors.hpp"
#include "wms/money.hpp"
#include "wms/permissions.hpp"
#include "wms/text.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace wms {

// ---------------------------------------------------------------------------
// Terminal helpers (the portable replacement for conio.h's getch()).
// ---------------------------------------------------------------------------

bool stdinIsTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

namespace {

// Turns terminal echo off for its lifetime and restores it afterwards.
class EchoGuard {
public:
    EchoGuard() {
#ifdef _WIN32
        handle_ = GetStdHandle(STD_INPUT_HANDLE);
        if (handle_ != INVALID_HANDLE_VALUE && GetConsoleMode(handle_, &oldMode_)) {
            active_ = SetConsoleMode(handle_, oldMode_ & ~static_cast<DWORD>(ENABLE_ECHO_INPUT)) != 0;
        }
#else
        if (tcgetattr(STDIN_FILENO, &old_) == 0) {
            termios silent = old_;
            silent.c_lflag &= ~static_cast<tcflag_t>(ECHO);
            active_ = tcsetattr(STDIN_FILENO, TCSANOW, &silent) == 0;
        }
#endif
    }
    ~EchoGuard() {
        if (!active_) return;
#ifdef _WIN32
        SetConsoleMode(handle_, oldMode_);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &old_);
#endif
    }
    EchoGuard(const EchoGuard&) = delete;
    EchoGuard& operator=(const EchoGuard&) = delete;

private:
    bool active_ = false;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    DWORD oldMode_ = 0;
#else
    termios old_{};
#endif
};

const char* const kRule = "==============================================================";
const char* const kThinRule = "--------------------------------------------------------------";

}  // namespace

std::optional<std::string> readPasswordFromTerminal(std::istream& in) {
    std::string line;
    if (&in == &std::cin && stdinIsTerminal()) {
        EchoGuard guard;
        if (!std::getline(in, line)) return std::nullopt;
    } else if (!std::getline(in, line)) {
        return std::nullopt;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

// ---------------------------------------------------------------------------
// Cli
// ---------------------------------------------------------------------------

Cli::Cli(Warehouse& warehouse, const UserStore& users, std::istream& in, std::ostream& out,
         PasswordReader readPassword, Options options)
    : warehouse_(warehouse),
      users_(users),
      in_(in),
      out_(out),
      readPassword_(std::move(readPassword)),
      options_(options) {}

std::optional<std::string> Cli::ask(const std::string& label) {
    out_ << label << std::flush;
    std::string line;
    if (!std::getline(in_, line)) {
        out_ << '\n';
        return std::nullopt;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (options_.echoInput) out_ << line << '\n';
    return text::trim(line);
}

std::optional<std::string> Cli::askPassword(const std::string& label) {
    out_ << label << std::flush;
    auto password = readPassword_(in_);
    if (options_.echoInput && password) out_ << std::string(password->size(), '*');
    out_ << '\n';  // the Enter key is not echoed while typing is hidden
    return password;
}

std::optional<int> Cli::askInt(const std::string& label) {
    while (true) {
        auto answer = ask(label);
        if (!answer) return std::nullopt;
        if (auto value = text::parseInt(*answer)) return value;
        out_ << "  Please enter a whole number.\n";
    }
}

std::optional<bool> Cli::askYesNo(const std::string& label) {
    while (true) {
        auto answer = ask(label + " (y/n): ");
        if (!answer) return std::nullopt;
        const std::string a = text::toLower(*answer);
        if (a == "y" || a == "yes") return true;
        if (a == "n" || a == "no") return false;
        out_ << "  Please answer y or n.\n";
    }
}

void Cli::printBanner() {
    out_ << kRule << "\n"
         << "                 WAREHOUSE MANAGEMENT SYSTEM\n"
         << kRule << "\n";
}

int Cli::run() {
    printBanner();
    while (true) {
        out_ << "\n  1. Log in\n  2. Quit\n";
        auto choice = askInt("\nChoice: ");
        if (!choice || *choice == 2) break;
        if (*choice != 1) {
            out_ << "  Please choose 1 or 2.\n";
            continue;
        }
        if (auto user = login()) session(*user);
        if (!in_) break;
    }
    out_ << "\nGoodbye.\n";
    return 0;
}

std::optional<User> Cli::login() {
    for (int attempt = 1; attempt <= options_.maxLoginAttempts; ++attempt) {
        auto username = ask("Username: ");
        if (!username) return std::nullopt;
        auto password = askPassword("Password: ");
        if (!password) return std::nullopt;

        if (auto user = users_.authenticate(*username, *password)) {
            out_ << "\n  Welcome, " << user->username << " (" << toString(user->role) << ").\n";
            return user;
        }
        const int left = options_.maxLoginAttempts - attempt;
        out_ << "  Invalid username or password.";
        if (left > 0) out_ << " " << left << " attempt(s) left.";
        out_ << "\n";
    }
    out_ << "  Too many failed attempts. Returning to the main menu.\n";
    return std::nullopt;
}

void Cli::session(const User& user) {
    const std::vector<Action> actions = allowedActions(user.role);
    std::string title = std::string(toString(user.role));
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    while (true) {
        out_ << "\n" << kThinRule << "\n  " << title << " MENU  (logged in as " << user.username
             << ")\n" << kThinRule << "\n";
        for (std::size_t i = 0; i < actions.size(); ++i) {
            out_ << "  " << (i + 1) << ". " << toString(actions[i]) << "\n";
        }
        const int logout = static_cast<int>(actions.size()) + 1;
        out_ << "  " << logout << ". Log out\n";

        auto choice = askInt("\nChoice: ");
        if (!choice) return;
        if (*choice == logout) {
            out_ << "  Logged out.\n";
            return;
        }
        if (*choice < 1 || *choice > logout) {
            out_ << "  Please choose a number from 1 to " << logout << ".\n";
            continue;
        }
        if (!runAction(user, actions[static_cast<std::size_t>(*choice - 1)])) return;
    }
}

bool Cli::runAction(const User& user, Action action) {
    try {
        switch (action) {
            case Action::ViewStock: showStock(user); return true;
            case Action::AddProduct: return addProduct(user);
            case Action::Refill: return refill(user);
            case Action::RemoveProduct: return removeProduct(user);
            case Action::Purchase: return purchase(user);
        }
    } catch (const WmsError& e) {
        out_ << "  Error: " << e.what() << "\n";
    }
    return static_cast<bool>(in_);
}

void Cli::showStock(const User& user) {
    const Inventory& inv = warehouse_.viewStock(user);
    out_ << "\n  " << std::left << std::setw(26) << "PRODUCT" << std::right << std::setw(12)
         << "UNIT PRICE" << std::setw(12) << "IN STOCK" << "\n  " << std::string(50, '-') << "\n";
    if (inv.empty()) {
        out_ << "  (no products in stock)\n";
        return;
    }
    for (const Stock& item : inv.items()) {
        out_ << "  " << std::left << std::setw(26) << item.name() << std::right << std::setw(12)
             << formatMoney(item.unitPrice()) << std::setw(12) << item.quantity() << "\n";
    }
    out_ << "  " << inv.size() << " product(s)\n";
}

bool Cli::addProduct(const User& user) {
    auto name = ask("Product name: ");
    if (!name) return false;
    auto price = ask("Unit price (e.g. 12.50): ");
    if (!price) return false;
    const Cents cents = parseMoney(*price);
    auto qty = askInt("Initial quantity: ");
    if (!qty) return false;
    warehouse_.addProduct(user, *name, cents, *qty);
    out_ << "  Added '" << Stock::normalizeName(*name) << "'.\n";
    return true;
}

bool Cli::refill(const User& user) {
    auto name = ask("Product name: ");
    if (!name) return false;
    auto qty = askInt("Units to add: ");
    if (!qty) return false;
    warehouse_.refill(user, *name, *qty);
    const Stock& item = warehouse_.inventory().get(*name);
    out_ << "  Refilled. '" << item.name() << "' now has " << item.quantity() << " unit(s).\n";
    return true;
}

bool Cli::removeProduct(const User& user) {
    auto name = ask("Product name: ");
    if (!name) return false;
    const Stock& item = warehouse_.inventory().get(*name);  // NotFoundError before confirming
    const std::string display = item.name();
    auto confirmed = askYesNo("Remove '" + display + "' permanently?");
    if (!confirmed) return false;
    if (!*confirmed) {
        out_ << "  Cancelled.\n";
        return true;
    }
    warehouse_.removeProduct(user, display);
    out_ << "  Removed '" << display << "'.\n";
    return true;
}

bool Cli::purchase(const User& user) {
    auto name = ask("Product name: ");
    if (!name) return false;
    auto qty = askInt("Quantity: ");
    if (!qty) return false;
    const Receipt quote = warehouse_.quote(user, *name, *qty);
    printReceipt(quote);
    auto confirmed = askYesNo("Confirm purchase?");
    if (!confirmed) return false;
    if (!*confirmed) {
        out_ << "  Purchase cancelled. Stock unchanged.\n";
        return true;
    }
    const Receipt done = warehouse_.purchase(user, *name, *qty);
    out_ << "  Payment of " << formatMoney(done.total) << " recorded. "
         << warehouse_.inventory().get(done.product).quantity() << " unit(s) of '"
         << done.product << "' left.\n";
    return true;
}

void Cli::printReceipt(const Receipt& r) {
    out_ << "\n  ORDER SUMMARY\n  " << std::string(36, '-') << "\n"
         << "  Product        " << r.product << "\n"
         << "  Quantity       " << r.quantity << " x " << formatMoney(r.unitPrice) << "\n"
         << "  Subtotal       " << formatMoney(r.subtotal) << "\n";
    if (r.discountPercent > 0) {
        out_ << "  Bulk discount  -" << formatMoney(r.discount) << " (" << r.discountPercent
             << "%)\n";
    } else {
        out_ << "  Bulk discount  none (5% off from 20 units, 10% from 50)\n";
    }
    out_ << "  Total due      " << formatMoney(r.total) << "\n  " << std::string(36, '-') << "\n";
}

}  // namespace wms
