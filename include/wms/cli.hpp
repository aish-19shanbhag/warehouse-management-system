// cli.hpp - the menu-driven terminal front end.
//
// The CLI reads from any std::istream and writes to any std::ostream, so the
// test suite drives complete sessions through string streams. There is no
// conio.h, no system("cls") and no goto: each menu is a loop in its own
// function.
#pragma once

#include <functional>
#include <iosfwd>
#include <optional>
#include <string>

#include "wms/auth.hpp"
#include "wms/warehouse.hpp"

namespace wms {

// Reads one password line from the stream; nullopt on end of input.
using PasswordReader = std::function<std::optional<std::string>(std::istream&)>;

// Default reader: hides typing when stdin is an interactive terminal
// (termios on Linux/macOS, SetConsoleMode on Windows), plain getline otherwise.
std::optional<std::string> readPasswordFromTerminal(std::istream& in);

// True when standard input is an interactive terminal.
bool stdinIsTerminal();

class Cli {
public:
    struct Options {
        // Echo each answer back to the output. Enabled automatically when input
        // is piped, so recorded sessions read like a real terminal. Passwords
        // are echoed as asterisks.
        bool echoInput = false;
        int maxLoginAttempts = 3;
    };

    Cli(Warehouse& warehouse, const UserStore& users, std::istream& in, std::ostream& out,
        PasswordReader readPassword, Options options);

    // Runs until the user quits or input ends. Returns the process exit code.
    int run();

private:
    std::optional<std::string> ask(const std::string& label);
    std::optional<std::string> askPassword(const std::string& label);
    std::optional<int> askInt(const std::string& label);
    std::optional<bool> askYesNo(const std::string& label);

    void printBanner();
    std::optional<User> login();
    void session(const User& user);
    bool runAction(const User& user, Action action);  // false when input ends

    void showStock(const User& user);
    bool addProduct(const User& user);
    bool refill(const User& user);
    bool removeProduct(const User& user);
    bool purchase(const User& user);
    void printReceipt(const Receipt& receipt);

    Warehouse& warehouse_;
    const UserStore& users_;
    std::istream& in_;
    std::ostream& out_;
    PasswordReader readPassword_;
    Options options_;
};

}  // namespace wms
