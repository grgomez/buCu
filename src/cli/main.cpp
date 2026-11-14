#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "bucu/platform/console.hpp"
#include "cli/run.hpp"

int main(int argc, char** argv) {
    // UTF-8 console output on Windows, restored when main returns (constitution principle 12).
    const bucu::platform::ConsoleSetup console;
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        return bucu::cli::run(args, std::cout, std::cerr);
    } catch (const std::exception& error) {
        // Reports the failure; nothing is swallowed (architecture section 7).
        std::cerr << "bucu: internal error: " << error.what() << '\n';
        return 1;
    }
}
