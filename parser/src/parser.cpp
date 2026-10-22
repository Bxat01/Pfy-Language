#include "pfy/parser.hpp"

#include "pfy/error.hpp"
#include "pfy/lexer.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace pfy {
namespace {

constexpr std::size_t kMaxNodes = 1'000'000;
constexpr std::size_t kMaxDepth = 256;

class Parser {
public:
    Parser(std::vector<Token> tokens, std::string filename)
        : tokens_(std::move(tokens)), filename_(std::move(filename)) {}

    NodePtr parseProgram() {
        const Token& header = current();
        if (header.kind != TokenKind::Identifier || header.text != "pfy") {
            fail(header, "expected the 'pfy' header");
        }
        advance();

        const Token& version = current();
        if (version.kind != TokenKind::Number || version.number != 1.0) {
            fail(version, "unsupported language version (expected pfy 1.0)");
        }
        advance();
        match(";");

        auto program = Node::make(NodeKind::Program, 1, 1);
        while (!atEnd()) {
            if (match(";")) continue;
            program->children.push_back(parseStatement());
        }
        validateTree(program);
        return program;
    }

private:
    std::vector<Token> tokens_;
    std::string filename_;
    std::size_t index_ = 0;
    std::size_t statementDepth_ = 0;
    std::size_t expressionDepth_ = 0;

    const Token& current() const { return tokens_[index_]; }
    const Token& peek(std::size_t ahead) const {
        std::size_t target = index_ + ahead;
        if (target >= tokens_.size()) return tokens_.back();
        return tokens_[target];
    }
    bool atEnd() const { return current().kind == TokenKind::End; }
    void advance() {
        if (!atEnd()) ++index_;
    }
    static bool isPunctuationText(const std::string& text) {
        return text.size() == 1 && std::string("{}[]():=,;+-*/%!<>.").find(text[0]) != std::string::npos ||
               text == "==" || text == "!=" || text == "<=" || text == ">=" || text == "&&" ||
               text == "||";
    }
    bool check(const std::string& text) const {
        if (atEnd() || current().text != text) return false;
        // A string literal may contain exactly the same characters as an
        // operator (for example "!"). Only a punct token can match syntax.
        if (isPunctuationText(text) && current().kind != TokenKind::Punct) return false;
        return true;
    }
    bool match(const std::string& text) {
        if (!check(text)) return false;
        advance();
        return true;
    }
    const Token& expect(const std::string& text, const std::string& message) {
        if (!check(text)) fail(current(), message);
        const Token& token = current();
        advance();
        return token;
    }

    [[noreturn]] void fail(const Token& token, const std::string& message) const {
        throw Error(message, token.line, token.column);
    }

    NodePtr parseStatement() {
        if (++statementDepth_ > kMaxDepth) {
            --statementDepth_;
            fail(current(), "statement nesting limit exceeded");
        }
        struct DepthGuard {
            std::size_t& depth;
            ~DepthGuard() { --depth; }
        } guard{statementDepth_};

        const Token& token = current();
        if (token.kind == TokenKind::Identifier) {
            if (token.text == "let") return parseLet();
            if (token.text == "set") return parseSet();
            if (token.text == "fn") return parseFunction();
            if (token.text == "if") return parseIf();
            if (token.text == "while") return parseWhile();
            if (token.text == "for") return parseFor();
            if (token.text == "return") return parseReturn();
            if (token.text == "break") return parseBreakOrContinue(NodeKind::Break);
            if (token.text == "continue") return parseBreakOrContinue(NodeKind::Continue);
        }
        NodePtr assignment = tryParseKeyAssignment();
        if (assignment) return assignment;
        return parseExpressionStatement();
    }

    NodePtr parseLet() {
        const Token& start = expect("let", "expected 'let'");
        const Token& name = expectIdentifier("expected a variable name after 'let'");
        if (!match("=")) fail(current(), "expected '=' after the variable name");
        auto node = Node::make(NodeKind::Let, start.line, start.column);
        node->text = name.text;
        node->children.push_back(parseExpression());
        expect(";", "expected ';' after the declaration");
        return node;
    }

    NodePtr parseSet() {
        const Token& start = expect("set", "expected 'set'");
        auto target = parsePostfix();
        if (!(target->kind == NodeKind::Identifier || target->kind == NodeKind::Member ||
              target->kind == NodeKind::Index)) {
            fail(current(), "the left side of 'set' must be a variable, member or index");
        }
        if (!match("=")) fail(current(), "expected '=' after the assignment target");
        auto node = Node::make(NodeKind::Set, start.line, start.column);
        node->children.push_back(target);
        node->children.push_back(parseExpression());
        expect(";", "expected ';' after the assignment");
        return node;
    }

    NodePtr parseFunction() {
        const Token& start = expect("fn", "expected 'fn'");
        const Token& name = expectIdentifier("expected a function name");
        auto node = Node::make(NodeKind::Function, start.line, start.column);
        node->text = name.text;
        expect("(", "expected '(' after the function name");
        if (!check(")")) {
            do {
                const Token& parameter = expectIdentifier("expected a parameter name");
                node->params.push_back(parameter.text);
            } while (match(","));
        }
        expect(")", "expected ')' after the parameter list");
        node->children.push_back(parseBlock());
        return node;
    }

    NodePtr parseIf() {
        const Token& start = expect("if", "expected 'if'");
        expect("(", "expected '(' after 'if'");
        auto node = Node::make(NodeKind::If, start.line, start.column);
        node->children.push_back(parseExpression());
        expect(")", "expected ')' after the condition");
        node->children.push_back(parseBlock());
        if (match("else")) {
            if (check("if")) {
                node->children.push_back(parseStatement());
            } else {
                node->children.push_back(parseBlock());
            }
        } else {
            node->children.push_back(Node::make(NodeKind::Null, start.line, start.column));
        }
        return node;
    }

    NodePtr parseWhile() {
        const Token& start = expect("while", "expected 'while'");
        expect("(", "expected '(' after 'while'");
        auto node = Node::make(NodeKind::While, start.line, start.column);
        node->children.push_back(parseExpression());
        expect(")", "expected ')' after the condition");
        node->children.push_back(parseBlock());
        return node;
    }

    NodePtr parseFor() {
        const Token& start = expect("for", "expected 'for'");
        expect("(", "expected '(' after 'for'");
        const Token& name = expectIdentifier("expected a loop variable");
        expect("in", "expected 'in' in the loop header");
        auto node = Node::make(NodeKind::For, start.line, start.column);
        node->text = name.text;
        node->children.push_back(parseExpression());
        expect(")", "expected ')' after the loop header");
        node->children.push_back(parseBlock());
        return node;
    }

    NodePtr parseReturn() {
        const Token& start = expect("return", "expected 'return'");
        auto node = Node::make(NodeKind::Return, start.line, start.column);
        if (check(";")) {
            node->children.push_back(Node::make(NodeKind::Null, start.line, start.column));
        } else {
            node->children.push_back(parseExpression());
        }
        expect(";", "expected ';' after 'return'");
        return node;
    }

    NodePtr parseBreakOrContinue(NodeKind kind) {
        const Token& start = current();
        advance();
        expect(";", "expected ';'");
        return Node::make(kind, start.line, start.column);
    }

    NodePtr parseExpressionStatement() {
        const Token& start = current();
        auto node = Node::make(NodeKind::Expression, start.line, start.column);
        node->children.push_back(parseExpression());
        expect(";", "expected ';' after the expression");
        return node;
    }

    NodePtr parseBlock() {
        const Token& start = expect("{", "expected '{'");
        auto node = Node::make(NodeKind::Block, start.line, start.column);
        while (!check("}")) {
            if (atEnd()) fail(current(), "unterminated block");
            if (match(";")) continue;
            node->children.push_back(parseStatement());
        }
        expect("}", "expected '}'");
        return node;
    }

    // A top-level identifier followed by '=' is a document field. Dotted field
    // names are allowed (Playfy shortcut ids such as console.toggle).
    NodePtr tryParseKeyAssignment() {
        const std::size_t saved = index_;
        if (current().kind != TokenKind::Identifier && current().kind != TokenKind::String) {
            return nullptr;
        }
        const Token& start = current();
        std::string key = current().text;
        advance();
        while (check(".")) {
            advance();
            if (current().kind != TokenKind::Identifier) {
                index_ = saved;
                return nullptr;
            }
            key += ".";
            key += current().text;
            advance();
        }
        if (!check("=")) {
            index_ = saved;
            return nullptr;
        }
        advance();
        auto target = Node::make(NodeKind::Identifier, start.line, start.column);
        target->text = key;
        auto node = Node::make(NodeKind::Set, start.line, start.column);
        node->children.push_back(target);
        node->children.push_back(parseExpression());
        expect(";", "expected ';' after the assignment");
        return node;
    }

    NodePtr parseExpression() {
        if (++expressionDepth_ > kMaxDepth) {
            --expressionDepth_;
            fail(current(), "expression nesting limit exceeded");
        }
        struct DepthGuard {
            std::size_t& depth;
            ~DepthGuard() { --depth; }
        } guard{expressionDepth_};
        return parseOr();
    }

    NodePtr parseOr() {
        auto left = parseAnd();
        while (match("||")) {
            auto node = Node::make(NodeKind::Binary, left->line, left->column);
            node->text = "||";
            node->children.push_back(left);
            node->children.push_back(parseAnd());
            left = node;
        }
        return left;
    }

    NodePtr parseAnd() {
        auto left = parseEquality();
        while (match("&&")) {
            auto node = Node::make(NodeKind::Binary, left->line, left->column);
            node->text = "&&";
            node->children.push_back(left);
            node->children.push_back(parseEquality());
            left = node;
        }
        return left;
    }

    NodePtr parseEquality() {
        auto left = parseComparison();
        while (check("==") || check("!=")) {
            std::string op = current().text;
            advance();
            auto node = Node::make(NodeKind::Binary, left->line, left->column);
            node->text = op;
            node->children.push_back(left);
            node->children.push_back(parseComparison());
            left = node;
        }
        return left;
    }

    NodePtr parseComparison() {
        auto left = parseTerm();
        while (check("<") || check("<=") || check(">") || check(">=")) {
            std::string op = current().text;
            advance();
            auto node = Node::make(NodeKind::Binary, left->line, left->column);
            node->text = op;
            node->children.push_back(left);
            node->children.push_back(parseTerm());
            left = node;
        }
        return left;
    }

    NodePtr parseTerm() {
        auto left = parseFactor();
        while (check("+") || check("-")) {
            std::string op = current().text;
            advance();
            auto node = Node::make(NodeKind::Binary, left->line, left->column);
            node->text = op;
            node->children.push_back(left);
            node->children.push_back(parseFactor());
            left = node;
        }
        return left;
    }

    NodePtr parseFactor() {
        auto left = parseUnary();
        while (check("*") || check("/") || check("%")) {
            std::string op = current().text;
            advance();
            auto node = Node::make(NodeKind::Binary, left->line, left->column);
            node->text = op;
            node->children.push_back(left);
            node->children.push_back(parseUnary());
            left = node;
        }
        return left;
    }

    NodePtr parseUnary() {
        std::vector<Token> operators;
        while (check("!") || check("-") || check("+")) {
            operators.push_back(current());
            advance();
            if (operators.size() > kMaxDepth) {
                fail(operators.back(), "expression nesting limit exceeded");
            }
        }
        auto value = parsePostfix();
        for (auto it = operators.rbegin(); it != operators.rend(); ++it) {
            auto node = Node::make(NodeKind::Unary, it->line, it->column);
            node->text = it->text;
            node->children.push_back(value);
            value = node;
        }
        return value;
    }

    NodePtr parsePostfix() {
        auto value = parsePrimary();
        while (true) {
            if (match("(")) {
                auto call = Node::make(NodeKind::Call, value->line, value->column);
                call->children.push_back(value);
                if (!check(")")) {
                    do {
                        call->children.push_back(parseExpression());
                    } while (match(","));
                }
                expect(")", "expected ')' after the arguments");
                value = call;
                continue;
            }
            if (match("[")) {
                auto index = Node::make(NodeKind::Index, value->line, value->column);
                index->children.push_back(value);
                index->children.push_back(parseExpression());
                expect("]", "expected ']' after the index");
                value = index;
                continue;
            }
            if (match(".")) {
                const Token& name = expectIdentifier("expected a member name after '.'");
                auto member = Node::make(NodeKind::Member, value->line, value->column);
                member->text = name.text;
                member->children.push_back(value);
                value = member;
                continue;
            }
            break;
        }
        return value;
    }

    NodePtr parsePrimary() {
        const Token& token = current();
        if (token.kind == TokenKind::Number) {
            auto node = Node::make(NodeKind::Number, token.line, token.column);
            node->number = token.number;
            advance();
            return node;
        }
        if (token.kind == TokenKind::String) {
            auto node = Node::make(NodeKind::String, token.line, token.column);
            node->text = token.text;
            advance();
            return node;
        }
        if (token.kind == TokenKind::Identifier) {
            if (token.text == "true" || token.text == "false") {
                auto node = Node::make(NodeKind::Bool, token.line, token.column);
                node->boolean = token.text == "true";
                advance();
                return node;
            }
            if (token.text == "null") {
                advance();
                return Node::make(NodeKind::Null, token.line, token.column);
            }
            auto node = Node::make(NodeKind::Identifier, token.line, token.column);
            node->text = token.text;
            advance();
            return node;
        }
        if (match("(")) {
            auto expression = parseExpression();
            expect(")", "expected ')' after the expression");
            return expression;
        }
        if (check("[")) return parseArray();
        if (check("{")) return parseObject();
        fail(token, "expected an expression");
    }

    NodePtr parseArray() {
        const Token& start = expect("[", "expected '['");
        auto node = Node::make(NodeKind::Array, start.line, start.column);
        while (!check("]")) {
            if (atEnd()) fail(current(), "unterminated array");
            if (match(",") || match(";")) continue;
            node->children.push_back(parseExpression());
            if (check("]")) break;
            if (!match(",") && !match(";")) {
                fail(current(), "expected ',' or ']' after the array value");
            }
        }
        expect("]", "expected ']'");
        return node;
    }

    NodePtr parseObject() {
        const Token& start = expect("{", "expected '{'");
        auto node = Node::make(NodeKind::Object, start.line, start.column);
        while (!check("}")) {
            if (atEnd()) fail(current(), "unterminated object");
            if (match(",") || match(";")) continue;
            std::string key = parseKey();
            if (!match(":") && !match("=")) {
                fail(current(), "expected ':' after the object key");
            }
            node->fields.emplace_back(std::move(key), parseExpression());
            if (check("}")) break;
            if (!match(",") && !match(";")) {
                fail(current(), "expected ',' or '}' after the object field");
            }
        }
        expect("}", "expected '}'");
        return node;
    }

    std::string parseKey() {
        if (current().kind == TokenKind::String) {
            std::string key = current().text;
            advance();
            return key;
        }
        if (current().kind != TokenKind::Identifier) {
            fail(current(), "expected an object key");
        }
        std::string key = current().text;
        advance();
        while (match(".")) {
            const Token& part = expectIdentifier("expected a name after '.' in the key");
            key += ".";
            key += part.text;
        }
        return key;
    }

    Token expectIdentifier(const std::string& message) {
        if (current().kind != TokenKind::Identifier) fail(current(), message);
        Token token = current();
        advance();
        return token;
    }

    void validateTree(const NodePtr& root) const {
        struct PendingNode {
            NodePtr node;
            std::size_t depth;
        };
        std::vector<PendingNode> pending{{root, 0}};
        std::size_t count = 0;
        while (!pending.empty()) {
            PendingNode currentNode = std::move(pending.back());
            pending.pop_back();
            if (currentNode.depth > kMaxDepth) {
                throw Error("AST is too deep", currentNode.node->line, currentNode.node->column);
            }
            if (++count > kMaxNodes) {
                throw Error("AST has too many nodes", currentNode.node->line, currentNode.node->column);
            }
            for (const auto& child : currentNode.node->children) {
                pending.push_back({child, currentNode.depth + 1});
            }
            for (const auto& field : currentNode.node->fields) {
                pending.push_back({field.second, currentNode.depth + 1});
            }
        }
    }
};

} // namespace

NodePtr parse(const std::string& source, const std::string& filename) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize(), filename);
    return parser.parseProgram();
}

} // namespace pfy
