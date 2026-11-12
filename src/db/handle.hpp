#pragma once

#include "bucu/db/database.hpp"

struct sqlite3;

namespace bucu::db::detail {

// The connection that Database::open configured. For code and tests inside the db module only:
// no other module runs SQL (architecture section 4).
inline sqlite3* handle(Database& database) { return database.connection_.get(); }

}  // namespace bucu::db::detail
