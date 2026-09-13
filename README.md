# Warehouse Management System

An object-oriented warehouse inventory management system built in C++, supporting three distinct user roles with role-based access control.

## Overview

This system models a real-world warehouse workflow: suppliers stocking inventory, customers purchasing items, and pickers managing fulfillment, each with a password-gated menu scoped to their responsibilities.

## Team Project

This was built as part of a 4-person team project for a Software Engineering course. My contributions centered on the core inventory logic and system testing.

## Key Features

- **Object-oriented design**: a core `stock` class encapsulates product data and inventory operations (add, withdraw, refill, remove)
- **Role-based access control**: three user types (Supplier, Customer, Picker), each with password-gated menus and permissions scoped to their responsibilities
- **File I/O-based persistence**: stock records persist across sessions without requiring a database
- **Test coverage**: 29 documented unit test cases across all user modules, achieving an 82.8% pass rate

## Tech Stack

- C++
- File-based I/O for data persistence

## Files

- `warehouse_management.cpp` — full system source code

## Running the Project

```bash
g++ warehouse_management.cpp -o warehouse_management
./warehouse_management
```

## Author

Aishwarya Ramanath Shanbhag
