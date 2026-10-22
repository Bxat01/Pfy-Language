#pragma once

#include "pfy/ast.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pfy {

struct BinaryImage {
    std::string source;
    NodePtr program;
};

// A .pfyb image contains a verified copy of the source and the serialized AST.
// Keeping the source makes diagnostics useful without re-reading the original
// file; the AST is the actual executable payload.
std::vector<std::uint8_t> encode(const NodePtr& program, const std::string& source);
BinaryImage decode(const std::vector<std::uint8_t>& bytes);

std::uint32_t crc32(const std::uint8_t* data, std::size_t size);

} // namespace pfy
