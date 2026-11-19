#pragma once

#include <chrono>
#include <functional>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "bucu/platform/environment.hpp"
#include "bucu/platform/paths.hpp"
#include "bucu/platform/terminal.hpp"

namespace bucu::cli {

// What a run needs from the world, so tests inject it (architecture section 9). `main` builds the
// real one.
struct Context {
    platform::Os os = platform::Os::kLinuxOrBsd;
    std::reference_wrapper<const platform::Environment> environment;
    std::chrono::sys_seconds now;
    std::reference_wrapper<std::istream> input;  // standard input
    std::reference_wrapper<std::ostream> out;    // standard output: normal output
    std::reference_wrapper<std::ostream> err;    // standard error: messages
    platform::StandardStreams terminals;         // whether standard input and output are terminals
};

// Runs bucu with the command-line arguments (without the program name), writes normal output to
// `context.out` and messages to `context.err`, and returns the exit code (product section 6).
int run(const std::vector<std::string>& args, const Context& context);

}  // namespace bucu::cli
