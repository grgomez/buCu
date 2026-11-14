#pragma once

#include <cstdint>

#include "bucu/platform/environment.hpp"

namespace bucu::platform {

// The value of the global `--color` option (docs/product.md section 5).
enum class ColorMode {
    kAuto,
    kAlways,
    kNever,
};

// Whether a standard stream is attached to a terminal. It is a type of its own, so it cannot be
// mixed up with another boolean or an integer.
enum class TerminalStatus : std::uint8_t {
    kTerminal,
    kNotTerminal,
};

// The terminal status of standard input and of standard output. Named members keep the two from
// being swapped.
struct StandardStreams {
    TerminalStatus input;
    TerminalStatus output;
};

// Whether output uses colour. `kAuto` means colour only when standard output is a terminal and
// NO_COLOR is unset or empty; `kAlways` and `kNever` override both.
bool use_color(ColorMode mode, TerminalStatus output, const Environment& environment);

// Whether the run is interactive (docs/product.md section 2): both streams are terminals.
bool is_interactive(StandardStreams streams);

// Whether standard input or standard output is attached to a terminal.
TerminalStatus stdin_is_terminal();
TerminalStatus stdout_is_terminal();

}  // namespace bucu::platform
