#include "bucu/platform/terminal.hpp"

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

}  // namespace bucu::platform
