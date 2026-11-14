#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "bucu/platform/environment.hpp"
#include "bucu/platform/paths.hpp"

namespace bucu::cli {

// What a run needs from the world, so tests inject it (architecture section 9). `main` builds the
// real one.
struct Context {
    platform::Os os;
    const platform::Environment& environment;
    std::int64_t now;  // Unix seconds, UTC
};

// Runs bucu with the command-line arguments (without the program name), writes normal output to
// `out` and messages to `err`, and returns the exit code (product section 6).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): out and err are distinct streams by design
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err,
        const Context& context);

}  // namespace bucu::cli
