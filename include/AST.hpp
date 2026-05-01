#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cmath>

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual double evaluate() = 0;
    virtual std::string toString(int indent = 0) = 0;
};

class NumberNode : public ASTNode {
public:
    double value;
    NumberNode(double val) : value(val) {}
    double evaluate() override { return value; }
    std::string toString(int indent = 0) override {
        return std::string(indent, ' ') + "Number: " + std::to_string(value);
    }
};

class BinaryOpNode : public ASTNode {
public:
    std::string op;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    BinaryOpNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
        : op(op), left(std::move(left)), right(std::move(right)) {}
    
    double evaluate() override {
        if (op == "+") return left->evaluate() + right->evaluate();
        if (op == "-") return left->evaluate() - right->evaluate();
        if (op == "*") return left->evaluate() * right->evaluate();
        if (op == "/") return left->evaluate() / right->evaluate();
        if (op == "^") return std::pow(left->evaluate(), right->evaluate());
        if (op == "%") return std::fmod(left->evaluate(), right->evaluate());
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
public:
    std::unique_ptr<ASTNode> expression;
    ReturnNode(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}
    double evaluate() override { return expression->evaluate(); }
    std::string toString(int indent = 0) override {
        return std::string(indent, ' ') + "Return:\n" + expression->toString(indent + 2);
    }
};

class UnaryFunctionNode : public ASTNode {
public:
    std::string funcName;
    std::unique_ptr<ASTNode> argument;
    UnaryFunctionNode(std::string name, std::unique_ptr<ASTNode> arg) : funcName(name), argument(std::move(arg)) {}
    double evaluate() override {
        double argVal = argument->evaluate();
        if (funcName == "sin") return std::sin(argVal);
        if (funcName == "cos") return std::cos(argVal);
        if (funcName == "tan") return std::tan(argVal);
        if (funcName == "sqrt") return std::sqrt(argVal);
        if (funcName == "log") return std::log(argVal);
        return 0;
    }
    std::string toString(int indent = 0) override {
        return std::string(indent, ' ') + "Function: " + funcName + "\n" + argument->toString(indent + 2);
    }
};

class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<ASTNode>> statements;
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
