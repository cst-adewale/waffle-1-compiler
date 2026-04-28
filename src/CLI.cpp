#include <iostream>
#include <string>
#include "../include/Lexer.hpp"
#include "../include/Parser.hpp"

int main() {
    std::string input;
    std::cout << "Waffle 1.0 Shell" << std::endl;
    std::cout << "Type 'exit' to quit." << std::endl;

    while (true) {
        std::cout << "waffle> ";
        std::getline(std::cin, input);

        if (input == "exit") break;
        if (input.empty()) continue;

        try {
            Lexer lexer(input);
            auto tokens = lexer.tokenize();

            // Optional: Print tokens for visualization
            std::cout << "[Tokens]: ";
            for (const auto& t : tokens) {
                if (t.type == WToken::END_OF_FILE) break;
                std::cout << "[" << t.value << "] ";
            }
            std::cout << std::endl;

            Parser parser(tokens);
            auto ast = parser.parse();

            // Optional: Print tree for visualization
            std::cout << "[AST]:\n" << ast->toString() << std::endl;

            std::cout << "Result: " << ast->evaluate() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    return 0;
}
