// Fuzz target for the UTF-8 rules of core: is_valid_utf8, decode_utf8, and to_lower.
// Besides the sanitizer reports, it aborts when an invariant of the contracts breaks.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#include "core/unicode_lower.hpp"
#include "core/utf8.hpp"

// libFuzzer requires this exact entry-point name.
// NOLINTNEXTLINE(readability-identifier-naming)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // libFuzzer passes bytes as uint8_t; std::string_view takes char.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const bool valid = bucu::core::is_valid_utf8(text);

    // decode_utf8 has a value exactly when the text is valid.
    if (bucu::core::decode_utf8(text).has_value() != valid) {
        std::abort();
    }
    // The lowercase mapping of valid UTF-8 is valid UTF-8.
    if (valid && !bucu::core::is_valid_utf8(bucu::core::to_lower(text))) {
        std::abort();
    }
    return 0;
}
