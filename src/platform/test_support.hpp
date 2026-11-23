#pragma once

// Operating-system calls that tests need, kept here so no test holds an #ifdef (architecture
// section 9). Internal: not in include/, and linked only into the test executable.
#include <optional>
#include <string_view>

namespace bucu::platform::test {

// Sets a process environment variable to UTF-8 text.
void set_environment_variable(std::string_view name, std::string_view value);

// Removes a process environment variable.
void unset_environment_variable(std::string_view name);

// Whether a console is attached to the process. Always false outside Windows.
bool console_attached();

// The console output code page, or 0 when no console is attached. Always 0 outside Windows.
unsigned int console_output_code_page();

// The console mode flags of standard output, or no value when standard output is not a console.
// Always no value outside Windows.
std::optional<unsigned int> console_output_mode();

}  // namespace bucu::platform::test
