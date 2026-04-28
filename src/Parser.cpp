#include "Parser.hpp"
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), position(0) {}

Token Parser::currentToken() {
    return tokens[position];
}

void Parser::eat(TokenType type) {
    if (currentToken().type == type) {
        position++;
    } else {
        throw std::runtime_error("Unexpected token");
    }
}

std::unique_ptr<ASTNode> Parser::factor() {
    Token token = currentToken();
    if (token.type == TokenType::NUMBER) {
        eat(TokenType::NUMBER);
        return std::make_unique<NumberNode>(std::stod(token.value));
    } else if (token.type == TokenType::LPAREN) {
        eat(TokenType::LPAREN);
        auto node = expression();
        eat(TokenType::RPAREN);
        return node;
    }
    throw std::runtime_error("Expected number or parenthesis");
}

std::unique_ptr<ASTNode> Parser::term() {
    auto node = factor();

    while (currentToken().type == TokenType::MUL || currentToken().type == TokenType::DIV) {
        Token token = currentToken();
        if (token.type == TokenType::MUL) {
            eat(TokenType::MUL);
        } else if (token.type == TokenType::DIV) {
            eat(TokenType::DIV);
        }
        node = std::make_unique<BinaryOpNode>(token.value, std::move(node), factor());
    }

    return node;
}

std::unique_ptr<ASTNode> Parser::expression() {
    auto node = term();

    while (currentToken().type == TokenType::PLUS || currentToken().type == TokenType::MINUS) {
        Token token = currentToken();
        if (token.type == TokenType::PLUS) {
            eat(TokenType::PLUS);
        } else if (token.type == TokenType::MINUS) {
            eat(TokenType::MINUS);
        }
        node = std::make_unique<BinaryOpNode>(token.value, std::move(node), term());
    }

    return node;
}

std::unique_ptr<ASTNode> Parser::parse() {
    return expression();
}
