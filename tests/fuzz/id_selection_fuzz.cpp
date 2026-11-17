// Fuzz target for ID argument parsing in core: parse_id_selection and covered_ranges.
// The input is split at each LF into the arguments. Besides the sanitizer reports, it aborts when
// an accepted selection breaks its documented shape.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "bucu/core/id_selection.hpp"

namespace {

std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos) {
            end = text.size();
        }
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

}  // namespace

// libFuzzer requires this exact entry-point name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // libFuzzer passes bytes as uint8_t; std::string_view takes char.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const auto selection = bucu::core::parse_id_selection(split_lines(text));
    if (!selection) {
        return 0;
    }
    const auto ranges = bucu::core::covered_ranges(*selection);
    for (std::size_t index = 0; index < ranges.size(); ++index) {
        const auto& range = ranges[index];
        const bool ordered = range.first.value() >= 1 && range.first <= range.last;
        const bool apart = index == 0 || ranges[index - 1].last.value() < range.first.value() - 1;
        if (!ordered || !apart) {
            std::abort();
        }
    }
    return 0;
}
