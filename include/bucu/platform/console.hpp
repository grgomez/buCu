#pragma once

namespace bucu::platform {

// On Windows, for as long as the object lives: puts the console output in UTF-8, and puts the
// previous code page back when it is destroyed, or when Ctrl+C or Ctrl+Break ends the process; and
// puts standard output and standard error in untranslated (binary) mode, with or without a console,
// so a line that ends in LF is written as LF (docs/product.md section 5, constitution principle
// 12). It does nothing on other platforms, and the code page part does nothing when no console is
// attached. `main` creates one.
class ConsoleSetup {
public:
    ConsoleSetup();
    ConsoleSetup(const ConsoleSetup&) = delete;
    ConsoleSetup& operator=(const ConsoleSetup&) = delete;
    ~ConsoleSetup();

private:
    // Used only on Windows; the other platforms leave it at 0 (-Wunused-private-field there).
    [[maybe_unused]] unsigned int previous_code_page_ = 0;  // 0 when nothing was changed
    [[maybe_unused]] int previous_stdout_mode_ = -1;        // -1 when nothing was changed
    [[maybe_unused]] int previous_stderr_mode_ = -1;
};

}  // namespace bucu::platform
