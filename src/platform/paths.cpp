#include "bucu/platform/paths.hpp"

#include <string>
#include <string_view>

namespace bucu::platform {

std::filesystem::path path_from_utf8(std::string_view text) {
    // char8_t and char share size and alignment; the bytes are the same UTF-8 text.
    const std::u8string utf8(text.begin(), text.end());
    return std::filesystem::path(utf8);
}

std::string utf8_from_path(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

}  // namespace bucu::platform
