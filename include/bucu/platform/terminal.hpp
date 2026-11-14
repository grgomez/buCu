#pragma once

#include "bucu/platform/environment.hpp"

namespace bucu::platform {

// The value of the global `--color` option (docs/product.md section 5).
enum class ColorMode {
    kAuto,
    kAlways,
    kNever,
};

// Whether output uses colour. `kAuto` means colour only when standard output is a terminal and
// NO_COLOR is unset or empty; `kAlways` and `kNever` override both.
bool use_color(ColorMode mode, bool stdout_is_terminal, const Environment& environment);

// Whether the run is interactive (docs/product.md section 2): both streams are terminals.
bool is_interactive(bool stdin_is_terminal, bool stdout_is_terminal);

// Whether standard input or standard output is attached to a terminal.
bool stdin_is_terminal();
bool stdout_is_terminal();

}  // namespace bucu::platform
