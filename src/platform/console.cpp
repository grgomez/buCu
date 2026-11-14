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

// The code page to put back on Ctrl+C or Ctrl+Break. Written before the handler is registered; the
// handler runs on another thread and only reads it.
std::atomic<UINT> g_code_page_to_restore{0};

BOOL WINAPI restore_on_interrupt(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        SetConsoleOutputCP(g_code_page_to_restore.load());
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
    if (previous_code_page_ != 0) {
        SetConsoleCtrlHandler(restore_on_interrupt, FALSE);
        SetConsoleOutputCP(previous_code_page_);
    }
#endif
}

}  // namespace bucu::platform
