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
    return {WToken::NUMBER, result};
}

Token Lexer::identifier() {
    std::string result = "";
    while (std::isalnum(currentChar())) {
        result += currentChar();
        advance();
    }
    if (result == "int") return {WToken::INT, result};
    if (result == "main") return {WToken::MAIN, result};
    if (result == "return") return {WToken::RETURN, result};
    return {WToken::IDENTIFIER, result};
}

Token Lexer::nextToken() {
    skipWhitespace();

    char current = currentChar();

    if (current == '\0') {
        return {WToken::END_OF_FILE, ""};
    }

    if (std::isdigit(current)) {
        return number();
    }

    if (std::isalpha(current)) {
        return identifier();
    }

    switch (current) {
        case '#': advance(); return {WToken::HASH, "#"};
        case '{': advance(); return {WToken::LBRACE, "{"};
        case '}': advance(); return {WToken::RBRACE, "}"};
        case '+': advance(); return {WToken::PLUS, "+"};
        case '-': advance(); return {WToken::MINUS, "-"};
        case '*': advance(); return {WToken::MUL, "*"};
        case '/': advance(); return {WToken::DIV, "/"};
        case '(': advance(); return {WToken::LPAREN, "("};
        case ')': advance(); return {WToken::RPAREN, ")"};
        case ';': advance(); return {WToken::SEMICOLON, ";"};
        default:
            std::string s(1, current);
            advance();
            return {WToken::UNKNOWN, s};
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    Token token = nextToken();
    while (token.type != WToken::END_OF_FILE) {
        tokens.push_back(token);
        token = nextToken();
    }
    tokens.push_back(token); // Add EOF
    return tokens;
}
