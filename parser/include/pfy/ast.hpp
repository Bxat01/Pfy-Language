#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace pfy {

// Tags are part of the .pfyb format. Do not renumber existing values; append
// new node kinds at the end and bump the format version in binary.cpp.
enum class NodeKind : std::uint8_t {
    Program = 1,
    Block = 2,
    Null = 3,
    Bool = 4,
    Number = 5,
    String = 6,
    Identifier = 7,
    Array = 8,
    Object = 9,
    Unary = 10,
    Binary = 11,
    Call = 12,
    Index = 13,
    Member = 14,
    Function = 15,
    Let = 16,
    Set = 17,
    If = 18,
    While = 19,
    For = 20,
    Return = 21,
    Break = 22,
    Continue = 23,
    Expression = 24,
};

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
    NodeKind kind = NodeKind::Null;
    int line = 1;
    int column = 1;
    std::string text;
    double number = 0.0;
    bool boolean = false;
    std::vector<NodePtr> children;
    std::vector<std::pair<std::string, NodePtr>> fields;
    std::vector<std::string> params;

    static NodePtr make(NodeKind kind, int line = 1, int column = 1) {
        auto node = std::make_shared<Node>();
        node->kind = kind;
        node->line = line;
        node->column = column;
        return node;
    }
};

const char* nodeKindName(NodeKind kind);

} // namespace pfy
