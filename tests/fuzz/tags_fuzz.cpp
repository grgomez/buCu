// Fuzz target for tag parsing in core: parse_tags.
// Besides the sanitizer reports, it aborts when a returned tag is not valid UTF-8.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "bucu/core/tags.hpp"
#include "core/utf8.hpp"

// libFuzzer requires this exact entry-point name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // libFuzzer passes bytes as uint8_t; std::string_view takes char.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const auto tags = bucu::core::parse_tags(text);
    if (!tags) {
        return 0;
    }
    for (const auto& tag : *tags) {
        if (!bucu::core::is_valid_utf8(tag)) {
            std::abort();
        }
    }
    return 0;
}
