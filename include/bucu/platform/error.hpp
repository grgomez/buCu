#pragma once

#include <cstdint>
#include <string>

namespace bucu::platform {

// Why a platform operation failed. `cli` maps each code to a message and an exit code.
enum class ErrorCode : std::uint8_t {
    kMissingVariable,        // a variable the default location needs is unset or empty
    kMissingDirectory,       // an override's parent directory is missing or is not a directory
    kCannotCreateDirectory,  // the default directory cannot be created
};

struct Error {
    ErrorCode code;
    std::string message;
};

}  // namespace bucu::platform
