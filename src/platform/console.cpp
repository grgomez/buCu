#include "bucu/platform/console.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace bucu::platform {

ConsoleSetup::ConsoleSetup() {
#ifdef _WIN32
    const UINT previous = GetConsoleOutputCP();  // 0 when no console is attached
    if (previous != 0 && previous != CP_UTF8 && SetConsoleOutputCP(CP_UTF8) != 0) {
        previous_code_page_ = previous;
    }
#endif
}

ConsoleSetup::~ConsoleSetup() {
#ifdef _WIN32
    if (previous_code_page_ != 0) {
        SetConsoleOutputCP(previous_code_page_);
    }
#endif
}

}  // namespace bucu::platform
