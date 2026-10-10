// errors.hpp - exception hierarchy used across the warehouse system.
//
// Every failure the application can recover from derives from WmsError, so the
// CLI can catch one type, print a friendly message and keep running.
#pragma once

#include <stdexcept>
#include <string>

namespace wms {

class WmsError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Bad user input: negative quantity, malformed price, empty product name, ...
class ValidationError : public WmsError {
public:
    using WmsError::WmsError;
};

// A product or user that does not exist.
class NotFoundError : public WmsError {
public:
    using WmsError::WmsError;
};

// A withdrawal or purchase asks for more units than are in stock.
class InsufficientStockError : public WmsError {
public:
    using WmsError::WmsError;
};

// The logged-in role is not allowed to perform the requested action.
class PermissionDeniedError : public WmsError {
public:
    using WmsError::WmsError;
};

// A data file exists but cannot be parsed (reports the line number).
class ParseError : public WmsError {
public:
    using WmsError::WmsError;
};

// A data file cannot be opened, written or replaced.
class StorageError : public WmsError {
public:
    using WmsError::WmsError;
};

}  // namespace wms
