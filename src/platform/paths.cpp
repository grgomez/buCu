#include "bucu/platform/paths.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "bucu/platform/environment.hpp"
#include "bucu/platform/error.hpp"

namespace bucu::platform {

namespace {

constexpr std::string_view kAppDirectory = "bucu";
constexpr std::string_view kDatabaseFile = "bookmarks.db";

// The value of a variable, with an empty value read as absent (docs/product.md section 3).
std::optional<std::string> non_empty(const Environment& environment, std::string_view name) {
    std::optional<std::string> value = environment.get(name);
    if (value.has_value() && value->empty()) {
        return std::nullopt;
    }
    return value;
}

// An absolute path in the XDG Base Directory sense: it starts with a slash.
bool is_absolute_xdg(const std::string& path) { return !path.empty() && path.front() == '/'; }

Error missing_variable(std::string_view name) {
    return Error{ErrorCode::kMissingVariable,
                 "the " + std::string(name) +
                     " environment variable is not set; set it, or name the "
                     "database with --db or BUCU_DB"};
}

std::expected<std::string, Error> default_path(Os os, const Environment& environment) {
    std::string separator = "/";
    std::string base;
    switch (os) {
        case Os::kWindows: {
            separator = "\\";
            const std::optional<std::string> app_data = non_empty(environment, "APPDATA");
            if (!app_data.has_value()) {
                return std::unexpected(missing_variable("APPDATA"));
            }
            base = *app_data;
            break;
        }
        case Os::kMacOs: {
            const std::optional<std::string> home = non_empty(environment, "HOME");
            if (!home.has_value()) {
                return std::unexpected(missing_variable("HOME"));
            }
            base = *home + "/Library/Application Support";
            break;
        }
        case Os::kLinuxOrBsd: {
            const std::optional<std::string> xdg = non_empty(environment, "XDG_DATA_HOME");
            if (xdg.has_value() && is_absolute_xdg(*xdg)) {
                base = *xdg;
            } else {
                const std::optional<std::string> home = non_empty(environment, "HOME");
                if (!home.has_value()) {
                    return std::unexpected(missing_variable("HOME"));
                }
                base = *home + "/.local/share";
            }
            break;
        }
    }
    return base + separator + std::string(kAppDirectory) + separator + std::string(kDatabaseFile);
}

}  // namespace

Os current_os() {
#if defined(_WIN32)
    return Os::kWindows;
#elif defined(__APPLE__)
    return Os::kMacOs;
#else
    return Os::kLinuxOrBsd;
#endif
}

std::expected<Location, Error> resolve_database_location(
    Os os, const Environment& environment, const std::optional<std::string>& explicit_path) {
    if (explicit_path.has_value() && !explicit_path->empty()) {
        return Location{*explicit_path, false};
    }
    if (const std::optional<std::string> override_path = non_empty(environment, "BUCU_DB")) {
        return Location{*override_path, false};
    }
    const std::expected<std::string, Error> path = default_path(os, environment);
    if (!path.has_value()) {
        return std::unexpected(path.error());
    }
    return Location{*path, true};
}

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
