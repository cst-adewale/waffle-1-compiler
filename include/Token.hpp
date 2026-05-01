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
    POWER,
    MODULO,
    SIN,
    COS,
    TAN,
    SQRT,
    LOG,
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    WToken type;
    std::string value;
    int start;
    int length;
};
