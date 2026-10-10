// permissions.hpp - roles, actions and the single permission matrix.
//
// In 2021 each role's permissions were implied by which menu a password
// unlocked. Here they are one table, consulted by every operation through
// requirePermission(), so the CLI menus and the business logic cannot drift
// apart.
//
//                 View  Add  Refill  Remove  Purchase
//   Supplier       x     x     x       x
//   Customer       x                            x
//   Picker         x           x
#pragma once

#include <optional>
#include <string_view>
#include <vector>

namespace wms {

enum class Role { Supplier, Customer, Picker };

// Declaration order is also the order actions appear in the role menus.
enum class Action { ViewStock, AddProduct, Refill, RemoveProduct, Purchase };

inline constexpr int kRoleCount = 3;
inline constexpr int kActionCount = 5;

bool isAllowed(Role role, Action action) noexcept;

// Throws PermissionDeniedError if the role may not perform the action.
void requirePermission(Role role, Action action);

// Actions the role may perform, in menu order.
std::vector<Action> allowedActions(Role role);

std::string_view toString(Role role) noexcept;
std::string_view toString(Action action) noexcept;

// Accepts "supplier", "Customer", "PICKER", ...
std::optional<Role> parseRole(std::string_view text);

}  // namespace wms
