#include "bucu/db/maintenance.hpp"

#include <utility>

#include "db/handle.hpp"
#include "db/statement.hpp"

namespace bucu::db {

std::expected<std::vector<std::string>, Error> integrity_check(Database& database) {
    auto statement = Statement::prepare(detail::handle(database), "PRAGMA integrity_check");
    if (!statement) {
        return std::unexpected(std::move(statement.error()));
    }

    std::vector<std::string> report;
    for (;;) {
        const auto row = statement->step();
        if (!row) {
            // A damaged file gives rows and then a failure; the user needs both (a problem the
            // check cannot even describe is still a problem).
            report.push_back(row.error().message);
            break;
        }
        if (!*row) {
            break;
        }
        report.push_back(statement->column_text(0));
    }

    if (report.size() == 1 && report.front() == "ok") {
        report.clear();
    }
    return report;
}

}  // namespace bucu::db
