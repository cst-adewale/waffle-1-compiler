#include "Parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), position(0) {}

Token Parser::currentToken() {
    return tokens[position];
}

void Parser::eat(WToken type) {
    if (currentToken().type == type) {
        position++;
    } else {
        throw std::runtime_error("Unexpected token");
    }
}

std::unique_ptr<ASTNode> Parser::factor() {
    Token token = currentToken();
    if (token.type == WToken::NUMBER) {
        eat(WToken::NUMBER);
        return std::make_unique<NumberNode>(std::stod(token.value));
    } else if (token.type == WToken::LPAREN) {
        eat(WToken::LPAREN);
        auto node = expression();
        eat(WToken::RPAREN);
        return node;
    }
    throw std::runtime_error("Expected number or parenthesis");
}

std::unique_ptr<ASTNode> Parser::term() {
    auto node = factor();

    while (currentToken().type == WToken::MUL || currentToken().type == WToken::DIV) {
        Token token = currentToken();
        if (token.type == WToken::MUL) {
            eat(WToken::MUL);
        } else if (token.type == WToken::DIV) {
            eat(WToken::DIV);
        }
        node = std::make_unique<BinaryOpNode>(token.value, std::move(node), factor());
    }

    return node;
}

std::unique_ptr<ASTNode> Parser::expression() {
    auto node = term();

    while (currentToken().type == WToken::PLUS || currentToken().type == WToken::MINUS) {
        Token token = currentToken();
        if (token.type == WToken::PLUS) {
            eat(WToken::PLUS);
        } else if (token.type == WToken::MINUS) {
            eat(WToken::MINUS);
        }
        node = std::make_unique<BinaryOpNode>(token.value, std::move(node), term());
    }

    return node;
}

std::unique_ptr<ASTNode> Parser::parse() {
    auto node = expression();
    if (currentToken().type == WToken::SEMICOLON) {
        eat(WToken::SEMICOLON);
    }
    return node;
}
