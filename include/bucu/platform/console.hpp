#pragma once

namespace bucu::platform {

// On Windows, for as long as the object lives: puts the console output in UTF-8 and turns on escape
// sequence processing for standard output when it is a console (so the colours of docs/product.md
// section 5 show), and puts the previous code page and console mode back when it is destroyed, or
// when Ctrl+C or Ctrl+Break ends the process; and puts standard output and standard error in
// untranslated (binary) mode, with or without a console, so a line that ends in LF is written as LF
// (docs/product.md section 5, constitution principle 12). It does nothing on other platforms, and
// the code page and console mode parts do nothing when no console is attached. `main` creates one.
class ConsoleSetup {
public:
    ConsoleSetup();
    ConsoleSetup(const ConsoleSetup&) = delete;
    ConsoleSetup& operator=(const ConsoleSetup&) = delete;
    ConsoleSetup(ConsoleSetup&&) = delete;
    ConsoleSetup& operator=(ConsoleSetup&&) = delete;
    ~ConsoleSetup();

private:
    // Used only on Windows; the other platforms leave it at 0 (-Wunused-private-field there).
    [[maybe_unused]] unsigned int previous_code_page_ = 0;  // 0 when nothing was changed
    [[maybe_unused]] int previous_stdout_mode_ = -1;        // -1 when nothing was changed
    [[maybe_unused]] int previous_stderr_mode_ = -1;
    [[maybe_unused]] unsigned int previous_output_console_mode_ = 0;
    [[maybe_unused]] bool output_console_mode_changed_ = false;
};

}  // namespace bucu::platform
