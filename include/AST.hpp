#pragma once
#include <string>
#include <memory>

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual double evaluate() = 0;
    virtual std::string toString(int indent = 0) = 0;
};

class NumberNode : public ASTNode {
    double value;
public:
    NumberNode(double val) : value(val) {}
    double evaluate() override { return value; }
    std::string toString(int indent = 0) override {
        return std::string(indent, ' ') + "Number: " + std::to_string(value);
    }
};

class BinaryOpNode : public ASTNode {
    std::string op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
public:
    BinaryOpNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
        : op(op), left(std::move(left)), right(std::move(right)) {}
    
    double evaluate() override {
        if (op == "+") return left->evaluate() + right->evaluate();
        if (op == "-") return left->evaluate() - right->evaluate();
        if (op == "*") return left->evaluate() * right->evaluate();
        if (op == "/") return left->evaluate() / right->evaluate();
        return 0;
    }

    std::string toString(int indent = 0) override {
        std::string s = std::string(indent, ' ') + "BinaryOp: " + op + "\n";
        s += left->toString(indent + 2) + "\n";
        s += right->toString(indent + 2);
        return s;
    }
};

class ReturnNode : public ASTNode {
    std::unique_ptr<ASTNode> expression;
public:
    ReturnNode(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}
    double evaluate() override { return expression->evaluate(); }
    std::string toString(int indent = 0) override {
        return std::string(indent, ' ') + "Return:\n" + expression->toString(indent + 2);
    }
};

class ProgramNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> statements;
public:
    void addStatement(std::unique_ptr<ASTNode> stmt) { statements.push_back(std::move(stmt)); }
    double evaluate() override {
        double lastVal = 0;
        for (auto& stmt : statements) {
            lastVal = stmt->evaluate();
        }
        return lastVal;
    }
    std::string toString(int indent = 0) override {
        std::string s = std::string(indent, ' ') + "Program:\n";
        for (auto& stmt : statements) {
            s += stmt->toString(indent + 2) + "\n";
        }
        return s;
    }
};
