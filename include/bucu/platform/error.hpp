#pragma once

#include <string>

namespace bucu::platform {

// Why a platform operation failed. `cli` maps each code to a message and an exit code.
enum class ErrorCode {
    kMissingVariable,
};

struct Error {
    ErrorCode code;
    std::string message;
};

}  // namespace bucu::platform
