#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "bucu/core/error.hpp"
#include "bucu/db/error.hpp"
#include "bucu/platform/error.hpp"

namespace bucu::cli {

// One error line, without the trailing LF (docs/product.md section 6).

// `bucu: invalid <field> '<value>': <reason>.` The value is left out for a title, a description,
// and text that is not valid UTF-8 (product section 6).
std::string validation_error_message(const core::ValidationError& error);

// The normalized URL is already stored as bookmark `id`.
std::string duplicate_url_message(std::string_view url, std::int64_t id);

// A database that cannot be opened: names the file.
std::string open_error_message(std::string_view file, const db::Error& error);

// A location that cannot be resolved or prepared: the platform message names the variable or
// directory.
std::string location_error_message(const platform::Error& error);

// A backup that cannot be written to `file`: names the file and the reason.
std::string backup_error_message(std::string_view file, const db::Error& error);

// A statement that failed while `action` ("store the bookmark") ran.
std::string sql_error_message(std::string_view action, const db::Error& error);

}  // namespace bucu::cli
