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
    int start = position;
    std::string result = "";
    bool hasDot = false;
    while (std::isdigit(currentChar()) || currentChar() == '.') {
        if (currentChar() == '.') {
            if (hasDot) break;
            hasDot = true;
        }
        result += currentChar();
        advance();
    }
    return {WToken::NUMBER, result, start, (int)result.length()};
}

Token Lexer::identifier() {
    int start = position;
    std::string result = "";
    while (std::isalnum(currentChar())) {
        result += currentChar();
        advance();
    }
    WToken type = WToken::IDENTIFIER;
    if (result == "int") type = WToken::INT;
    else if (result == "main") type = WToken::MAIN;
    else if (result == "return") type = WToken::RETURN;
    else if (result == "sin") type = WToken::SIN;
    else if (result == "cos") type = WToken::COS;
    else if (result == "tan") type = WToken::TAN;
    else if (result == "sqrt") type = WToken::SQRT;
    else if (result == "log") type = WToken::LOG;
    
    return {type, result, start, (int)result.length()};
}

Token Lexer::nextToken() {
    skipWhitespace();

    int start = position;
    char current = currentChar();

    if (current == '\0') {
        return {WToken::END_OF_FILE, "", start, 0};
    }

    if (std::isdigit(current)) {
        return number();
    }

    if (std::isalpha(current)) {
        return identifier();
    }

    advance();
    std::string s(1, current);
    WToken type = WToken::UNKNOWN;
    switch (current) {
        case '#': type = WToken::HASH; break;
        case '{': type = WToken::LBRACE; break;
        case '}': type = WToken::RBRACE; break;
        case '+': type = WToken::PLUS; break;
        case '-': type = WToken::MINUS; break;
        case '*': type = WToken::MUL; break;
        case '/': type = WToken::DIV; break;
        case '^': type = WToken::POWER; break;
        case '%': type = WToken::MODULO; break;
        case '(': type = WToken::LPAREN; break;
        case ')': type = WToken::RPAREN; break;
        case ';': type = WToken::SEMICOLON; break;
    }
    return {type, s, start, 1};
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
