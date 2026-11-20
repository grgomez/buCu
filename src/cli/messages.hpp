#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "bucu/core/bookmark_id.hpp"
#include "bucu/core/error.hpp"
#include "bucu/db/error.hpp"
#include "bucu/platform/error.hpp"

namespace bucu::cli {

// One error line, without the trailing LF (docs/product.md section 6).

// `bucu: invalid <field> '<value>': <reason>.` The value is left out for a title, a description,
// and text that is not valid UTF-8 (product section 6).
std::string validation_error_message(const core::ValidationError& error);

// The normalized URL is already stored as bookmark `id`.
std::string duplicate_url_message(std::string_view url, core::BookmarkId id);

// An ID argument that is not a positive whole number (product section 5, IDs and ranges). The
// argument is shown with each control character written as an escape.
std::string invalid_id_message(std::string_view argument);

// A usage error: `bucu: <what failed>. Run 'bucu --help' for usage.` `what` has no final period.
std::string usage_error_message(std::string_view what);

// A usage error: `word` is neither an ID nor a tag operation. The word is shown with each control
// character written as an escape.
std::string not_an_operation_message(std::string_view word);

// No stored bookmark matches the IDs given: names the missing ID or the range with no stored ID,
// or says there are no bookmarks when `error` names neither (product section 6).
std::string not_found_message(const db::Error& error);

// A database that cannot be opened: names the file.
std::string open_error_message(std::string_view file, const db::Error& error);

// A location that cannot be resolved or prepared: the platform message names the variable or
// directory.
std::string location_error_message(const platform::Error& error);

// A backup that cannot be written to `file`: names the file and the reason.
std::string backup_error_message(std::string_view file, const db::Error& error);

// `Deleted <count> bookmarks.`, or `Deleted 1 bookmark.` for one (product section 5, delete).
std::string deleted_message(std::int64_t count);

// A statement that failed while `action` ("store the bookmark") ran.
std::string sql_error_message(std::string_view action, const db::Error& error);

}  // namespace bucu::cli
