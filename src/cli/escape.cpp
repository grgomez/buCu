#include "cli/escape.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace bucu::cli {

std::string escape_controls(std::string_view text, LineFeed line_feed) {
    constexpr std::string_view kHex = "0123456789abcdef";
    constexpr unsigned kNibbleBits = 4;
    constexpr unsigned kNibbleMask = 0xF;
    constexpr unsigned char kFirstPrintable = 0x20;  // below it are the C0 controls
    constexpr unsigned char kDelete = 0x7F;
    std::string escaped;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        const bool c1 = byte == 0xC2 && index + 1 < text.size() &&
                        static_cast<unsigned char>(text[index + 1]) >= 0x80 &&
                        static_cast<unsigned char>(text[index + 1]) <= 0x9F;
        if (c1) {
            const auto next = static_cast<unsigned char>(text[++index]);
            escaped += "\\u00";
            escaped += kHex[next >> kNibbleBits];
            escaped += kHex[next & kNibbleMask];
        } else if (byte == '\n' && line_feed == LineFeed::kKeep) {
            escaped += '\n';
        } else if (byte == '\n') {
            escaped += "\\n";
        } else if (byte == '\r') {
            escaped += "\\r";
        } else if (byte == '\t') {
            escaped += "\\t";
        } else if (byte < kFirstPrintable || byte == kDelete) {
            escaped += "\\x";
            escaped += kHex[byte >> kNibbleBits];
            escaped += kHex[byte & kNibbleMask];
        } else {
            escaped += static_cast<char>(byte);
        }
    }
    return escaped;
}

}  // namespace bucu::cli
