#include <chrono>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "bucu/platform/console.hpp"
#include "bucu/platform/environment.hpp"
#include "bucu/platform/paths.hpp"
#include "cli/run.hpp"

int main(int argc, char** argv) {
    // UTF-8 console output on Windows, restored when main returns (constitution principle 12).
    const bucu::platform::ConsoleSetup console;
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        const bucu::platform::SystemEnvironment environment;
        const auto now =
            std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
        const bucu::cli::Context context{bucu::platform::current_os(), environment,
                                         now.time_since_epoch().count()};
        return bucu::cli::run(args, std::cout, std::cerr, context);
    } catch (const std::exception& error) {
        // Reports the failure; nothing is swallowed (architecture section 7).
        std::cerr << "bucu: internal error: " << error.what() << '\n';
        return 1;
    }
}
