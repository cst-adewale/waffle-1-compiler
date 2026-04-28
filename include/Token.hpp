#pragma once
#include <string>

enum class WToken {
    NUMBER,
    IDENTIFIER,
    INT,
    RETURN,
    MAIN,
    HASH,
    LBRACE,
    RBRACE,
    PLUS,
    MINUS,
    MUL,
    DIV,
    LPAREN,
    RPAREN,
    SEMICOLON,
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    WToken type;
    std::string value;
};
