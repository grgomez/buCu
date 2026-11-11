#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace bucu::cli {

// Runs bucu with the command-line arguments (without the program name), writes normal output to
// `out` and messages to `err`, and returns the exit code (product section 6).
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace bucu::cli
