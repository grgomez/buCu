#include <filesystem>
#include <ostream>
#include <string>
#include <system_error>

#include "bucu/db/error.hpp"
#include "bucu/db/maintenance.hpp"
#include "bucu/platform/paths.hpp"
#include "cli/commands.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"

namespace bucu::cli {

int run_db_path(const Globals& globals, const Context& context, std::ostream& out,
                std::ostream& err) {
    const auto location =
        platform::resolve_database_location(context.os, context.environment, globals.db);
    if (!location) {
        err << location_error_message(location.error()) << '\n';
        return kExitFailure;
    }
    std::error_code status;
    const std::filesystem::path absolute =
        std::filesystem::absolute(platform::path_from_utf8(location->path), status);
    if (status) {
        err << "bucu: cannot make " << location->path << " absolute: " << status.message() << ".\n";
        return kExitFailure;
    }
    out << platform::utf8_from_path(absolute) << '\n';
    return kExitOk;
}

int run_db_version(const Globals& globals, const Context& context, std::ostream& out,
                   std::ostream& err) {
    const auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    out << "schema " << database->schema_version() << " (newest supported: " << db::kSchemaVersion
        << ")\n";
    return kExitOk;
}

int run_db_check(const Globals& globals, const Context& context, std::ostream& out,
                 std::ostream& err) {
    auto database = open_database(globals, context, err);
    if (!database) {
        return kExitFailure;
    }
    const auto report = db::integrity_check(*database);
    if (!report) {
        err << sql_error_message("check the database", report.error()) << '\n';
        return kExitFailure;
    }
    if (report->empty()) {
        out << "ok\n";
        return kExitOk;
    }
    // The report as SQLite gives it: a problem may hold line breaks.
    for (const std::string& problem : *report) {
        out << problem << '\n';
    }
    err << "bucu: the database failed its integrity check. Restore a backup, or export what is "
           "readable.\n";
    return kExitFailure;
}

}  // namespace bucu::cli
