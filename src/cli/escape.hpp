#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace bucu::cli {

// What `escape_controls` does with LF.
enum class LineFeed : std::uint8_t {
    kEscape,  // written as `\n`, so the text stays on one line
    kKeep,    // left as it is, for a value that continues on the next line
};

// `text` with each control character written as an escape (`\n`, `\x1b`, `\u0085`), so a value
// echoed in a message or a record stays safe to print: it sends no control byte to the terminal.
// Expects UTF-8. LF is kept or escaped as `line_feed` says.
std::string escape_controls(std::string_view text, LineFeed line_feed = LineFeed::kEscape);

}  // namespace bucu::cli
