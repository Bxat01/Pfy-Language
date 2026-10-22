#include "pfy/lexer.hpp"

#include "pfy/ast.hpp"
#include "pfy/error.hpp"

#include <cctype>
#include <cstdint>
#include <sstream>

namespace pfy {
namespace {

bool isIdentStart(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

bool isIdentContinue(char c) {
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

void appendUtf8(std::string& out, std::uint32_t code) {
    if (code <= 0x7F) {
        out.push_back(static_cast<char>(code));
    } else if (code <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (code >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {
    // Editors on Windows commonly add a UTF-8 BOM. It is not part of the
    // language and must not turn into an "unexpected character" error.
    if (source_.size() >= 3 && static_cast<unsigned char>(source_[0]) == 0xEF &&
        static_cast<unsigned char>(source_[1]) == 0xBB &&
        static_cast<unsigned char>(source_[2]) == 0xBF) {
        source_.erase(0, 3);
    }
}

bool Lexer::atEnd() const { return offset_ >= source_.size(); }

char Lexer::peek(std::size_t ahead) const {
    if (offset_ + ahead >= source_.size()) return '\0';
    return source_[offset_ + ahead];
}

char Lexer::advance() {
    if (atEnd()) return '\0';
    char c = source_[offset_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

Token Lexer::make(TokenKind kind, std::string text, int line, int column) const {
    Token token;
    token.kind = kind;
    token.text = std::move(text);
    token.line = line;
    token.column = column;
    return token;
}

void Lexer::skipSpaceAndComments() {
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        if (c == '#') {
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            int startLine = line_;
            int startColumn = column_;
            advance();
            advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (atEnd()) throw Error("unterminated block comment", startLine, startColumn);
            advance();
            advance();
            continue;
        }
        break;
    }
}

Token Lexer::readString() {
    int startLine = line_;
    int startColumn = column_;
    advance(); // opening quote
    std::string value;
    while (!atEnd()) {
        char c = advance();
        if (c == '"') return make(TokenKind::String, std::move(value), startLine, startColumn);
        if (c == '\n') throw Error("newline inside string", startLine, startColumn);
        if (c != '\\') {
            value.push_back(c);
            continue;
        }
        if (atEnd()) throw Error("unterminated escape sequence", startLine, startColumn);
        char escaped = advance();
        switch (escaped) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u': {
                std::uint32_t code = 0;
                for (int i = 0; i < 4; ++i) {
                    if (atEnd()) throw Error("short unicode escape", startLine, startColumn);
                    int digit = hexValue(advance());
                    if (digit < 0) throw Error("invalid unicode escape", startLine, startColumn);
                    code = (code << 4) | static_cast<std::uint32_t>(digit);
                }
                // Combine a UTF-16 surrogate pair when present. Unpaired
                // surrogates are preserved as the replacement character so
                // malformed input cannot create invalid UTF-8.
                if (code >= 0xD800 && code <= 0xDBFF && peek() == '\\' && peek(1) == 'u') {
                    advance();
                    advance();
                    std::uint32_t low = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (atEnd()) throw Error("short unicode escape", startLine, startColumn);
                        int digit = hexValue(advance());
                        if (digit < 0) throw Error("invalid unicode escape", startLine, startColumn);
                        low = (low << 4) | static_cast<std::uint32_t>(digit);
                    }
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    } else {
                        code = 0xFFFD;
                    }
                } else if (code >= 0xD800 && code <= 0xDFFF) {
                    code = 0xFFFD;
                }
                appendUtf8(value, code);
                break;
            }
            default:
                throw Error(std::string("unknown escape \\") + escaped, startLine, startColumn);
        }
    }
    throw Error("unterminated string", startLine, startColumn);
}

Token Lexer::readNumber() {
    int startLine = line_;
    int startColumn = column_;
    std::size_t start = offset_;

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance();
        advance();
        std::size_t digitsStart = offset_;
        while (!atEnd() && hexValue(peek()) >= 0) advance();
        if (offset_ == digitsStart) throw Error("expected hexadecimal digits", startLine, startColumn);
        std::string digits = source_.substr(digitsStart, offset_ - digitsStart);
        std::uint64_t value = 0;
        for (char c : digits) value = value * 16 + static_cast<std::uint64_t>(hexValue(c));
        Token token = make(TokenKind::Number, source_.substr(start, offset_ - start), startLine, startColumn);
        token.number = static_cast<double>(value);
        return token;
    }

    while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        advance();
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
    }
    if (peek() == 'e' || peek() == 'E') {
        std::size_t mark = offset_;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!std::isdigit(static_cast<unsigned char>(peek()))) {
            offset_ = mark;
        } else {
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
    }

    std::string raw = source_.substr(start, offset_ - start);
    std::istringstream stream(raw);
    double value = 0.0;
    stream >> value;
    if (!stream || !stream.eof()) throw Error("invalid number", startLine, startColumn);
    Token token = make(TokenKind::Number, raw, startLine, startColumn);
    token.number = value;
    return token;
}

Token Lexer::readIdentifier() {
    int startLine = line_;
    int startColumn = column_;
    std::size_t start = offset_;
    advance();
    while (!atEnd() && isIdentContinue(peek())) advance();
    return make(TokenKind::Identifier, source_.substr(start, offset_ - start), startLine, startColumn);
}

Token Lexer::readPunct() {
    int startLine = line_;
    int startColumn = column_;
    char first = advance();
    char second = peek();
    if ((first == '=' && second == '=') || (first == '!' && second == '=') ||
        (first == '<' && second == '=') || (first == '>' && second == '=') ||
        (first == '&' && second == '&') || (first == '|' && second == '|')) {
        advance();
        return make(TokenKind::Punct, std::string{first, second}, startLine, startColumn);
    }
    if (std::string("{}[]():=,;+-*/%!<>.").find(first) != std::string::npos) {
        return make(TokenKind::Punct, std::string(1, first), startLine, startColumn);
    }
    throw Error(std::string("unexpected character '") + first + "'", startLine, startColumn);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        skipSpaceAndComments();
        if (atEnd()) break;
        char c = peek();
        if (c == '"') {
            tokens.push_back(readString());
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(readNumber());
        } else if (isIdentStart(c)) {
            tokens.push_back(readIdentifier());
        } else {
            tokens.push_back(readPunct());
        }
    }
    tokens.push_back(make(TokenKind::End, "", line_, column_));
    return tokens;
}

const char* nodeKindName(NodeKind kind) {
    switch (kind) {
        case NodeKind::Program: return "program";
        case NodeKind::Block: return "block";
        case NodeKind::Null: return "null";
        case NodeKind::Bool: return "bool";
        case NodeKind::Number: return "number";
        case NodeKind::String: return "string";
        case NodeKind::Identifier: return "identifier";
        case NodeKind::Array: return "array";
        case NodeKind::Object: return "object";
        case NodeKind::Unary: return "unary";
        case NodeKind::Binary: return "binary";
        case NodeKind::Call: return "call";
        case NodeKind::Index: return "index";
        case NodeKind::Member: return "member";
        case NodeKind::Function: return "function";
        case NodeKind::Let: return "let";
        case NodeKind::Set: return "set";
        case NodeKind::If: return "if";
        case NodeKind::While: return "while";
        case NodeKind::For: return "for";
        case NodeKind::Return: return "return";
        case NodeKind::Break: return "break";
        case NodeKind::Continue: return "continue";
        case NodeKind::Expression: return "expression";
    }
    return "unknown";
}

} // namespace pfy
