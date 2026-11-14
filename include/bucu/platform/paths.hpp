#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace bucu::platform {

// Builds a path from UTF-8 text, so the same text names the same file on every platform
// (constitution principle 12).
std::filesystem::path path_from_utf8(std::string_view text);

// The path as UTF-8 text.
std::string utf8_from_path(const std::filesystem::path& path);

}  // namespace bucu::platform
