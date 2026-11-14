#include "bucu/platform/environment.hpp"

#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "platform/utf16.hpp"

namespace bucu::platform {

std::optional<std::string> SystemEnvironment::get(std::string_view name) const {
#ifdef _WIN32
    const std::wstring wide_name = wide_from_utf8(name);
    const wchar_t* value = _wgetenv(wide_name.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return utf8_from_wide(value);
#else
    const std::string c_name(name);
    const char* value = std::getenv(c_name.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

}  // namespace bucu::platform
