#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "bucu/platform/environment.hpp"
#include "bucu/platform/error.hpp"

namespace bucu::platform {

// The operating-system family whose data directory rules apply. BSD uses the Linux rules.
enum class Os {
    kLinuxOrBsd,
    kMacOs,
    kWindows,
};

// The family BuCu was built for.
Os current_os();

// Where the database is: UTF-8 text joined with the separator of the `Os` given to the resolver,
// and whether it is the default location (as opposed to an override).
struct Location {
    std::string path;
    bool is_default;
};

// Applies docs/product.md section 3: an explicit path, then BUCU_DB, then the default under the
// platform data directory. An empty value counts as absent.
std::expected<Location, Error> resolve_database_location(
    Os os, const Environment& environment, const std::optional<std::string>& explicit_path);

// Builds a path from UTF-8 text, so the same text names the same file on every platform
// (constitution principle 12).
std::filesystem::path path_from_utf8(std::string_view text);

// The path as UTF-8 text.
std::string utf8_from_path(const std::filesystem::path& path);

}  // namespace bucu::platform
