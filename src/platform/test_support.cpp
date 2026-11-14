#include "platform/test_support.hpp"

#include <cstdlib>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

#include "platform/utf16.hpp"

namespace bucu::platform::test {

void set_environment_variable(std::string_view name, std::string_view value) {
#ifdef _WIN32
    _wputenv_s(wide_from_utf8(name).c_str(), wide_from_utf8(value).c_str());
#else
    setenv(std::string(name).c_str(), std::string(value).c_str(), 1);
#endif
}

void unset_environment_variable(std::string_view name) {
#ifdef _WIN32
    // An empty value removes the variable on Windows.
    _wputenv_s(wide_from_utf8(name).c_str(), L"");
#else
    unsetenv(std::string(name).c_str());
#endif
}

bool console_attached() {
#ifdef _WIN32
    return GetConsoleOutputCP() != 0;
#else
    return false;
#endif
}

unsigned int console_output_code_page() {
#ifdef _WIN32
    return GetConsoleOutputCP();
#else
    return 0;
#endif
}

}  // namespace bucu::platform::test
