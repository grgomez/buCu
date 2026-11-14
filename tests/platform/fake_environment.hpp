#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "bucu/platform/environment.hpp"

namespace bucu::platform::test {

// An environment backed by a map, so no test depends on the machine's variables (architecture
// section 9).
class FakeEnvironment final : public Environment {
public:
    FakeEnvironment() = default;
    explicit FakeEnvironment(std::map<std::string, std::string, std::less<>> values)
        : values_(std::move(values)) {}

    std::optional<std::string> get(std::string_view name) const override {
        const auto found = values_.find(name);
        if (found == values_.end()) {
            return std::nullopt;
        }
        return found->second;
    }

private:
    std::map<std::string, std::string, std::less<>> values_;
};

}  // namespace bucu::platform::test
