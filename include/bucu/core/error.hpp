#pragma once

#include <cstdint>
#include <string>

namespace bucu::core {

// The bookmark field a validation error is about.
enum class Field : std::uint8_t {
    kUrl,
    kTag,
    kTitle,
    kDescription,
    kKeyword,
};

// Why a value was refused. `cli` maps each code to a message (product section 6).
enum class Reason : std::uint8_t {
    kEmpty,
    kInvalidUtf8,
    kNoScheme,
    kWhitespace,
    kControlCharacter,
    kEmptyHost,
    kTooLong,
};

// A rule of product section 4 that a value broke: the field, the reason, and the value as given.
struct ValidationError {
    Field field;
    Reason reason;
    std::string value;
};

}  // namespace bucu::core
