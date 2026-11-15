#include "bucu/platform/terminal.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <optional>
#include <string>

#include "bucu/platform/environment.hpp"

namespace bucu::platform {

bool use_color(ColorMode mode, TerminalStatus output, const Environment& environment) {
    switch (mode) {
        case ColorMode::kAlways:
            return true;
        case ColorMode::kNever:
            return false;
        case ColorMode::kAuto:
            break;
    }
    const std::optional<std::string> no_color = environment.get("NO_COLOR");
    const bool no_color_set = no_color.has_value() && !no_color->empty();
    return output == TerminalStatus::kTerminal && !no_color_set;
}

bool is_interactive(StandardStreams streams) {
    return streams.input == TerminalStatus::kTerminal &&
           streams.output == TerminalStatus::kTerminal;
}

namespace {

TerminalStatus status_of(bool attached) {
    return attached ? TerminalStatus::kTerminal : TerminalStatus::kNotTerminal;
}

}  // namespace

TerminalStatus stdin_is_terminal() {
#ifdef _WIN32
    return status_of(_isatty(_fileno(stdin)) != 0);
#else
    return status_of(isatty(STDIN_FILENO) != 0);
#endif
}

TerminalStatus stdout_is_terminal() {
#ifdef _WIN32
    return status_of(_isatty(_fileno(stdout)) != 0);
#else
    return status_of(isatty(STDOUT_FILENO) != 0);
#endif
}

}  // namespace bucu::platform
