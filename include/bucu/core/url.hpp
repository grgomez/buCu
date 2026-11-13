#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "bucu/core/error.hpp"

namespace bucu::core {

// Validate `url` and return its normalized form (product section 4), or the rule it broke.
std::expected<std::string, ValidationError> normalize_url(std::string_view url);

}  // namespace bucu::core
