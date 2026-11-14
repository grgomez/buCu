#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace bucu::platform {

// Read access to environment variables, as UTF-8. Tests pass a fake (architecture section 9).
class Environment {
public:
    Environment() = default;
    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    virtual ~Environment() = default;

    // The value of a variable, or no value when it is not set. A variable set to the empty string
    // returns the empty string; the rules that read it decide what empty means.
    virtual std::optional<std::string> get(std::string_view name) const = 0;
};

// The process environment. On Windows it reads through UTF-16 (constitution principle 12).
class SystemEnvironment final : public Environment {
public:
    std::optional<std::string> get(std::string_view name) const override;
};

}  // namespace bucu::platform
