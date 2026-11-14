#pragma once

// Operating-system calls that tests need, kept here so no test holds an #ifdef (architecture
// section 9). Internal: not in include/, and linked only into the test executable.
#include <string_view>

namespace bucu::platform::test {

// Sets a process environment variable to UTF-8 text.
void set_environment_variable(std::string_view name, std::string_view value);

// Removes a process environment variable.
void unset_environment_variable(std::string_view name);

}  // namespace bucu::platform::test
