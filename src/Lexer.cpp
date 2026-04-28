#include "Lexer.hpp"
#include <cctype>

Lexer::Lexer(const std::string& source) : source(source), position(0) {}

char Lexer::currentChar() {
    if (position >= source.length()) return '\0';
    return source[position];
}

void Lexer::advance() {
    position++;
}

void Lexer::skipWhitespace() {
    while (std::isspace(currentChar())) {
        advance();
    }
}

Token Lexer::number() {
    std::string result = "";
    while (std::isdigit(currentChar())) {
        result += currentChar();
        advance();
    }
    return {TokenType::NUMBER, result};
}

Token Lexer::nextToken() {
    skipWhitespace();

    char current = currentChar();

    if (current == '\0') {
        return {TokenType::END_OF_FILE, ""};
    }

    if (std::isdigit(current)) {
        return number();
    }

    switch (current) {
        case '+': advance(); return {TokenType::PLUS, "+"};
        case '-': advance(); return {TokenType::MINUS, "-"};
        case '*': advance(); return {TokenType::MUL, "*"};
        case '/': advance(); return {TokenType::DIV, "/"};
        case '(': advance(); return {TokenType::LPAREN, "("};
        case ')': advance(); return {TokenType::RPAREN, ")"};
        default:
            std::string s(1, current);
            advance();
            return {TokenType::UNKNOWN, s};
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    Token token = nextToken();
    while (token.type != TokenType::END_OF_FILE) {
        tokens.push_back(token);
        token = nextToken();
    }
    tokens.push_back(token); // Add EOF
    return tokens;
}
