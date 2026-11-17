// Fuzz target for URL validation in core: normalize_url.
// Besides the sanitizer reports, it aborts when a normalized URL is not valid UTF-8.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "bucu/core/url.hpp"
#include "core/utf8.hpp"

// libFuzzer requires this exact entry-point name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // libFuzzer passes bytes as uint8_t; std::string_view takes char.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const auto url = bucu::core::normalize_url(text);
    if (url && !bucu::core::is_valid_utf8(*url)) {
        std::abort();
    }
    return 0;
}
