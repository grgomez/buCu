#pragma once

#include "bucu/db/database.hpp"

struct sqlite3;

namespace bucu::db::detail {

// The connection that Database::open configured. For code and tests inside the db module only:
// no other module runs SQL (architecture section 4).
class Access {
public:
    static sqlite3* get(Database& database) { return database.connection_.get(); }
};

inline sqlite3* handle(Database& database) { return Access::get(database); }

}  // namespace bucu::db::detail
