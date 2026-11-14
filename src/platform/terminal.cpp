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

bool use_color(ColorMode mode, bool stdout_is_terminal, const Environment& environment) {
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
    return stdout_is_terminal && !no_color_set;
}

bool is_interactive(bool stdin_is_terminal, bool stdout_is_terminal) {
    return stdin_is_terminal && stdout_is_terminal;
}

bool stdin_is_terminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

bool stdout_is_terminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

}  // namespace bucu::platform
