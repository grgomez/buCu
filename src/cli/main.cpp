#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "cli/run.hpp"

int main(int argc, char** argv) {
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        return bucu::cli::run(args, std::cout, std::cerr);
    } catch (const std::exception& error) {
        // Reports the failure; nothing is swallowed (architecture section 7).
        std::cerr << "bucu: internal error: " << error.what() << '\n';
        return 1;
    }
}
