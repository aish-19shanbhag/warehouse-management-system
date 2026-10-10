// main.cpp - command-line entry point.
//
//   warehouse [--data DIR]                       start the interactive menu
//   warehouse [--data DIR] --add-user NAME ROLE  create an account (prompts for a password)
//   warehouse --help | --version
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "wms/auth.hpp"
#include "wms/cli.hpp"
#include "wms/errors.hpp"
#include "wms/warehouse.hpp"

namespace {

constexpr const char* kVersion = "2.0.0";

void printUsage(std::ostream& out) {
    out << "Usage:\n"
        << "  warehouse [--data DIR]                        start the interactive menu\n"
        << "  warehouse [--data DIR] --add-user NAME ROLE   create an account\n"
        << "  warehouse --help | --version\n\n"
        << "DIR holds inventory.csv and users.csv (default: ./data).\n"
        << "ROLE is one of: Supplier, Customer, Picker.\n";
}

int addUser(const std::filesystem::path& usersFile, const std::string& name,
            const std::string& roleText) {
    const auto role = wms::parseRole(roleText);
    if (!role) {
        std::cerr << "Unknown role '" << roleText << "'. Use Supplier, Customer or Picker.\n";
        return 2;
    }
    wms::UserStore store = wms::UserStore::loadCsv(usersFile);

    std::cout << "New password for " << name << ": " << std::flush;
    const auto first = wms::readPasswordFromTerminal(std::cin);
    std::cout << "\nRepeat password: " << std::flush;
    const auto second = wms::readPasswordFromTerminal(std::cin);
    std::cout << "\n";
    if (!first || !second) {
        std::cerr << "No password entered.\n";
        return 2;
    }
    if (*first != *second) {
        std::cerr << "Passwords do not match.\n";
        return 2;
    }
    const wms::User& user = store.addUser(name, *role, *first);
    store.saveCsv(usersFile);
    std::cout << "Created " << wms::toString(user.role) << " account '" << user.username
              << "' in " << usersFile.string() << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path dataDir = "data";
    std::vector<std::string> addUserArgs;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(std::cout);
            return 0;
        }
        if (arg == "--version") {
            std::cout << "warehouse " << kVersion << "\n";
            return 0;
        }
        if (arg == "--data" && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "--add-user" && i + 2 < argc) {
            addUserArgs = {argv[i + 1], argv[i + 2]};
            i += 2;
        } else {
            std::cerr << "Unrecognised argument: " << arg << "\n\n";
            printUsage(std::cerr);
            return 2;
        }
    }

    const std::filesystem::path usersFile = dataDir / "users.csv";
    const std::filesystem::path inventoryFile = dataDir / "inventory.csv";

    try {
        if (!addUserArgs.empty()) return addUser(usersFile, addUserArgs[0], addUserArgs[1]);

        const wms::UserStore users = wms::UserStore::loadCsv(usersFile);
        if (users.empty()) {
            std::cerr << "No accounts found in " << usersFile.string() << ".\n"
                      << "Create one with: warehouse --data " << dataDir.string()
                      << " --add-user NAME ROLE\n";
            return 1;
        }
        wms::Warehouse warehouse = wms::Warehouse::open(inventoryFile);

        wms::Cli::Options options;
        options.echoInput = !wms::stdinIsTerminal();
        wms::Cli cli(warehouse, users, std::cin, std::cout, wms::readPasswordFromTerminal, options);
        return cli.run();
    } catch (const wms::WmsError& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
