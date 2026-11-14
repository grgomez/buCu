#pragma once

#include <string>

namespace bucu::core {

// The bookmark field a validation error is about.
enum class Field {
    kUrl,
    kTag,
    kTitle,
    kDescription,
};

// Why a value was refused. `cli` maps each code to a message (product section 6).
enum class Reason {
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
