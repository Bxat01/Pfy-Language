#pragma once

#include <string>
#include <vector>

namespace pfy {

enum class TokenKind {
    End,
    Identifier,
    Number,
    String,
    Punct,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    double number = 0.0;
    int line = 1;
    int column = 1;
};

// Converts UTF-8 source text into tokens. Comments and whitespace are removed;
// strings remain decoded so later stages never have to understand escapes.
class Lexer {
public:
    explicit Lexer(std::string source);
    std::vector<Token> tokenize();

private:
    std::string source_;
    std::size_t offset_ = 0;
    int line_ = 1;
    int column_ = 1;

    bool atEnd() const;
    char peek(std::size_t ahead = 0) const;
    char advance();
    void skipSpaceAndComments();
    Token readString();
    Token readNumber();
    Token readIdentifier();
    Token readPunct();
    Token make(TokenKind kind, std::string text, int line, int column) const;
};

} // namespace pfy
