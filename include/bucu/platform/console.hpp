#pragma once

namespace bucu::platform {

// Puts the Windows console output in UTF-8 for as long as the object lives, and puts the previous
// code page back when it is destroyed, or when Ctrl+C or Ctrl+Break ends the process
// (constitution principle 12). It does nothing on other platforms, and nothing on Windows when no
// console is attached. `main` creates one.
class ConsoleSetup {
public:
    ConsoleSetup();
    ConsoleSetup(const ConsoleSetup&) = delete;
    ConsoleSetup& operator=(const ConsoleSetup&) = delete;
    ~ConsoleSetup();

private:
    // Used only on Windows; the other platforms leave it at 0 (-Wunused-private-field there).
    [[maybe_unused]] unsigned int previous_code_page_ = 0;  // 0 when nothing was changed
};

}  // namespace bucu::platform
