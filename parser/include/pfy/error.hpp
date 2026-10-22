#pragma once

#include <stdexcept>
#include <string>

namespace pfy {

// All user-facing compiler failures derive from this type so the CLI can print
// a source location instead of a C++ diagnostic.
class Error : public std::runtime_error {
public:
    Error(std::string message, int line, int column)
        : std::runtime_error(std::move(message)), line_(line), column_(column) {}

    int line() const noexcept { return line_; }
    int column() const noexcept { return column_; }

private:
    int line_;
    int column_;
};

class BinaryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace pfy
