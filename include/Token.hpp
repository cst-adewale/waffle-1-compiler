#pragma once
#include <string>

enum class WToken {
    NUMBER,
    PLUS,
    MINUS,
    MUL,
    DIV,
    LPAREN,
    RPAREN,
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    WToken type;
    std::string value;
};
