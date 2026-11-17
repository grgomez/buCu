#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "bucu/platform/console.hpp"
#include "bucu/platform/environment.hpp"
#include "bucu/platform/paths.hpp"
#include "bucu/platform/terminal.hpp"
#include "cli/run.hpp"

int main(int argc, char** argv) {
    // UTF-8 console output on Windows, restored when main returns (constitution principle 12).
    const bucu::platform::ConsoleSetup console;
    try {
        const std::span<char*> all(argv, static_cast<std::size_t>(argc));
        const auto rest = all.subspan(all.empty() ? 0 : 1);
        const std::vector<std::string> args(rest.begin(), rest.end());
        const bucu::platform::SystemEnvironment environment;
        const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
        const bucu::cli::Context context{
            .os = bucu::platform::current_os(),
            .environment = environment,
            .now = now,
            .input = std::cin,
            .terminals = {.input = bucu::platform::stdin_is_terminal(),
                          .output = bucu::platform::stdout_is_terminal()}};
        return bucu::cli::run(args, std::cout, std::cerr, context);
    } catch (const std::exception& error) {
        // Reports the failure; nothing is swallowed (architecture section 7).
        std::cerr << "bucu: internal error: " << error.what() << '\n';
        return 1;
    }
}
