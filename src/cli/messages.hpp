#pragma once

#include <string>
#include <string_view>

#include "bucu/db/error.hpp"
#include "bucu/platform/error.hpp"

namespace bucu::cli {

// One error line, without the trailing LF (docs/product.md section 6).

// A database that cannot be opened: names the file.
std::string open_error_message(std::string_view file, const db::Error& error);

// A location that cannot be resolved or prepared: the platform message names the variable or
// directory.
std::string location_error_message(const platform::Error& error);

// A statement that failed while `action` ("store the bookmark") ran.
std::string sql_error_message(std::string_view action, const db::Error& error);

}  // namespace bucu::cli
