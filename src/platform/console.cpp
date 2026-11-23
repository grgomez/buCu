#include "bucu/platform/console.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>

#include <atomic>
#include <cstdio>
#endif

namespace bucu::platform {

#ifdef _WIN32
namespace {

// The code page (0 means unchanged) and the console output mode (with its changed flag) to put back
// on Ctrl+C or Ctrl+Break. Written before the handler is registered; the handler runs on another
// thread and only reads them.
std::atomic<UINT> g_code_page_to_restore{0};
std::atomic<DWORD> g_output_mode_to_restore{0};
std::atomic<bool> g_output_mode_changed{false};  // a previous mode of 0 is valid, so 0 says nothing

BOOL WINAPI restore_on_interrupt(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        if (g_code_page_to_restore.load() != 0) {
            SetConsoleOutputCP(g_code_page_to_restore.load());
        }
        if (g_output_mode_changed.load()) {
            SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), g_output_mode_to_restore.load());
        }
    }
    return FALSE;  // The default handling then ends the process.
}

}  // namespace
#endif

ConsoleSetup::ConsoleSetup() {
#ifdef _WIN32
    // Without this, the C runtime writes CR LF for every LF to a file or a pipe. _setmode returns
    // the previous mode, or -1 when the stream is not open.
    previous_stdout_mode_ = _setmode(_fileno(stdout), _O_BINARY);
    previous_stderr_mode_ = _setmode(_fileno(stderr), _O_BINARY);
    const UINT previous = GetConsoleOutputCP();  // 0 when no console is attached
    if (previous != 0 && previous != CP_UTF8 && SetConsoleOutputCP(CP_UTF8) != 0) {
        previous_code_page_ = previous;
        g_code_page_to_restore.store(previous);
    }
    // Escape sequence processing, so colour codes are not printed as text. Only when standard
    // output is a console that has it off; when it fails, output still goes out, with raw codes.
    DWORD mode = 0;
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(output, &mode) != 0 && (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0 &&
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        previous_output_console_mode_ = mode;
        output_console_mode_changed_ = true;
        g_output_mode_to_restore.store(mode);
        g_output_mode_changed.store(true);
    }
    if (previous_code_page_ != 0 || output_console_mode_changed_) {
        SetConsoleCtrlHandler(restore_on_interrupt, TRUE);
    }
#endif
}

ConsoleSetup::~ConsoleSetup() {
#ifdef _WIN32
    if (previous_stdout_mode_ != -1) {
        _setmode(_fileno(stdout), previous_stdout_mode_);
    }
    if (previous_stderr_mode_ != -1) {
        _setmode(_fileno(stderr), previous_stderr_mode_);
    }
    if (previous_code_page_ != 0 || output_console_mode_changed_) {
        SetConsoleCtrlHandler(restore_on_interrupt, FALSE);
    }
    if (previous_code_page_ != 0) {
        SetConsoleOutputCP(previous_code_page_);
    }
    if (output_console_mode_changed_) {
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), previous_output_console_mode_);
    }
#endif
}

}  // namespace bucu::platform
