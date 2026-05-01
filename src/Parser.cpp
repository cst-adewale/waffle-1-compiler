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
    } else if (token.type == WToken::SIN || token.type == WToken::COS || 
               token.type == WToken::TAN || token.type == WToken::SQRT || 
               token.type == WToken::LOG) {
        std::string funcName = token.value;
        eat(token.type);
        eat(WToken::LPAREN);
        auto arg = expression();
        eat(WToken::RPAREN);
        return std::make_unique<UnaryFunctionNode>(funcName, std::move(arg));
    } else if (token.type == WToken::LPAREN) {
        eat(WToken::LPAREN);
        auto node = expression();
        eat(WToken::RPAREN);
        return node;
    }
    throw std::runtime_error("Expected number or parenthesis");
}

std::unique_ptr<ASTNode> Parser::power() {
    auto node = factor();
    if (currentToken().type == WToken::POWER) {
        Token token = currentToken();
        eat(WToken::POWER);
        node = std::make_unique<BinaryOpNode>(token.value, std::move(node), power());
    }
    return node;
}

std::unique_ptr<ASTNode> Parser::term() {
    auto node = power();

    while (currentToken().type == WToken::MUL || currentToken().type == WToken::DIV || currentToken().type == WToken::MODULO) {
        Token token = currentToken();
        if (token.type == WToken::MUL) {
            eat(WToken::MUL);
        } else if (token.type == WToken::DIV) {
            eat(WToken::DIV);
        } else if (token.type == WToken::MODULO) {
            eat(WToken::MODULO);
        }
        node = std::make_unique<BinaryOpNode>(token.value, std::move(node), power());
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

std::unique_ptr<ASTNode> Parser::statement() {
    if (currentToken().type == WToken::RETURN) {
        eat(WToken::RETURN);
        auto expr = expression();
        if (currentToken().type == WToken::SEMICOLON) eat(WToken::SEMICOLON);
        return std::make_unique<ReturnNode>(std::move(expr));
    }
    
    auto expr = expression();
    if (currentToken().type == WToken::SEMICOLON) eat(WToken::SEMICOLON);
    return expr;
}

std::unique_ptr<ASTNode> Parser::block() {
    auto prog = std::make_unique<ProgramNode>();
    eat(WToken::LBRACE);
    while (currentToken().type != WToken::RBRACE && currentToken().type != WToken::END_OF_FILE) {
        prog->addStatement(statement());
    }
    eat(WToken::RBRACE);
    return prog;
}

std::unique_ptr<ASTNode> Parser::parse() {
    auto prog = std::make_unique<ProgramNode>();
    
    while (currentToken().type != WToken::END_OF_FILE) {
        // Skip Preprocessor directives (#include ...)
        if (currentToken().type == WToken::HASH) {
            eat(WToken::HASH);
            while (currentToken().type != WToken::END_OF_FILE && 
                   currentToken().type != WToken::INT && 
                   currentToken().type != WToken::MAIN) {
                position++; // Just skip tokens until we hit 'int' or 'main'
            }
            continue;
        }

        // Handle 'int main() { ... }'
        if (currentToken().type == WToken::INT) {
            eat(WToken::INT);
            if (currentToken().type == WToken::MAIN) {
                eat(WToken::MAIN);
                eat(WToken::LPAREN);
                eat(WToken::RPAREN);
                prog->addStatement(block());
            } else {
                // Future: handle variable declarations
                prog->addStatement(statement());
            }
        } else {
            prog->addStatement(statement());
        }
    }
    
    return prog;
}
