#pragma once
#include "Token.hpp"
#include <vector>
#include <string>

class Lexer {
public:
    Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    std::string source;
    size_t position;
    char currentChar();
    void advance();
    void skipWhitespace();
    Token nextToken();
    Token number();
    Token identifier();
};
