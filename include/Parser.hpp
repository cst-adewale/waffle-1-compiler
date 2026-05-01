#pragma once
#include "Token.hpp"
#include "AST.hpp"
#include <vector>
#include <memory>

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ASTNode> parse();

private:
    std::vector<Token> tokens;
    size_t position;

    Token currentToken();
    void eat(WToken type);

    std::unique_ptr<ASTNode> expression();
    std::unique_ptr<ASTNode> term();
    std::unique_ptr<ASTNode> power();
    std::unique_ptr<ASTNode> factor();
    std::unique_ptr<ASTNode> statement();
    std::unique_ptr<ASTNode> block();
};
