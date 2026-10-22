#pragma once

#include "pfy/ast.hpp"

#include <string>

namespace pfy {

// Parses a complete Pfy document, including the `pfy 1.0;` header. The same
// parser handles data documents (Playfy backups) and executable scripts; a
// top-level `name = value;` is simply a module field.
NodePtr parse(const std::string& source, const std::string& filename = "<input>");

} // namespace pfy
