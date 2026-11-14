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
    wchar_t* value = nullptr;
    std::size_t length = 0;
    // _wdupenv_s copies the value, so it is safe against a concurrent change to the environment.
    if (_wdupenv_s(&value, &length, wide_name.c_str()) != 0 || value == nullptr) {
        return std::nullopt;
    }
    std::string utf8 = utf8_from_wide(value);
    free(value);
    return utf8;
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
