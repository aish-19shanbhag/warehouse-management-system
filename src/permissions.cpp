#include "wms/permissions.hpp"

#include <array>
#include <string>

#include "wms/errors.hpp"
#include "wms/text.hpp"

namespace wms {

namespace {

// The single source of truth for who may do what.
// Columns follow the Action enum: View, Add, Refill, Remove, Purchase.
constexpr std::array<std::array<bool, kActionCount>, kRoleCount> kPermissionMatrix{{
    /* Supplier */ {true, true, true, true, false},
    /* Customer */ {true, false, false, false, true},
    /* Picker   */ {true, false, true, false, false},
}};

constexpr std::array<Action, kActionCount> kAllActions{
    Action::ViewStock, Action::AddProduct, Action::Refill, Action::RemoveProduct,
    Action::Purchase};

constexpr std::array<Role, kRoleCount> kAllRoles{Role::Supplier, Role::Customer, Role::Picker};

}  // namespace

bool isAllowed(Role role, Action action) noexcept {
    const auto r = static_cast<std::size_t>(role);
    const auto a = static_cast<std::size_t>(action);
    if (r >= kPermissionMatrix.size() || a >= kPermissionMatrix[r].size()) return false;
    return kPermissionMatrix[r][a];
}

void requirePermission(Role role, Action action) {
    if (!isAllowed(role, action)) {
        throw PermissionDeniedError(std::string(toString(role)) + " accounts cannot perform '" +
                                    std::string(toString(action)) + "'");
    }
}

std::vector<Action> allowedActions(Role role) {
    std::vector<Action> actions;
    for (Action a : kAllActions) {
        if (isAllowed(role, a)) actions.push_back(a);
    }
    return actions;
}

std::string_view toString(Role role) noexcept {
    switch (role) {
        case Role::Supplier: return "Supplier";
        case Role::Customer: return "Customer";
        case Role::Picker: return "Picker";
    }
    return "Unknown";
}

std::string_view toString(Action action) noexcept {
    switch (action) {
        case Action::ViewStock: return "View stock";
        case Action::AddProduct: return "Add new product";
        case Action::Refill: return "Refill stock";
        case Action::RemoveProduct: return "Remove product";
        case Action::Purchase: return "Purchase";
    }
    return "Unknown";
}

std::optional<Role> parseRole(std::string_view text) {
    const std::string t = text::trim(text);
    for (Role r : kAllRoles) {
        if (text::iequals(t, toString(r))) return r;
    }
    return std::nullopt;
}

}  // namespace wms
