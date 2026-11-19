#pragma once

#include <chrono>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "bucu/platform/terminal.hpp"
#include "catch_amalgamated.hpp"
#include "cli/run.hpp"
#include "platform/fake_environment.hpp"
#include "support/sqlite_read.hpp"
#include "support/temp_dir.hpp"

namespace bucu::test::cli {

using Rows = std::vector<std::vector<std::string>>;

// What one run printed and returned.
struct Result {
    int code;
    std::string out;
    std::string err;
};

inline constexpr std::chrono::sys_seconds kNow{std::chrono::seconds{1700000000}};

inline constexpr platform::StandardStreams kPiped{.input = platform::TerminalStatus::kNotTerminal,
                                                  .output = platform::TerminalStatus::kNotTerminal};
inline constexpr platform::StandardStreams kTerminal{.input = platform::TerminalStatus::kTerminal,
                                                     .output = platform::TerminalStatus::kTerminal};

// A database file in a fresh directory, and `bucu` runs against it with a fixed clock. `input` is
// what standard input holds and `terminals` says whether the streams are terminals; both default
// to a piped, empty run.
struct Fixture {
    TempDir dir;
    platform::test::FakeEnvironment environment;
    std::filesystem::path db = dir.path() / "bookmarks.db";
    std::string input;
    platform::StandardStreams terminals = kPiped;

    [[nodiscard]] std::string db_text() const { return utf8_path(db); }

    // `bucu --db <db> <args>`
    [[nodiscard]] Result bucu(std::vector<std::string> args) const {
        args.insert(args.begin(), {"--db", db_text()});
        return invoke(args);
    }

    // `bucu <args>`, with no --db added.
    [[nodiscard]] Result invoke(const std::vector<std::string>& args) const {
        return invoke(args, environment, input, terminals);
    }

    static Result invoke(const std::vector<std::string>& args,
                         const platform::Environment& environment, const std::string& input = "",
                         platform::StandardStreams terminals = kPiped) {
        std::istringstream in(input);
        std::ostringstream out;
        std::ostringstream err;
        const bucu::cli::Context context{.os = platform::Os::kLinuxOrBsd,
                                         .environment = environment,
                                         .now = kNow,
                                         .input = in,
                                         .out = out,
                                         .err = err,
                                         .terminals = terminals};
        const int code = bucu::cli::run(args, context);
        return {.code = code, .out = out.str(), .err = err.str()};
    }

    [[nodiscard]] Rows rows(const char* sql) const { return read_rows(db, sql); }

    [[nodiscard]] Rows bookmarks() const {
        return rows(
            "SELECT id, url, title, description, flags, created_at, updated_at "
            "FROM bookmark ORDER BY id");
    }

    [[nodiscard]] std::string tags() const {
        std::string joined;
        for (const auto& row : rows("SELECT name FROM tag ORDER BY name")) {
            joined += (joined.empty() ? "" : ",") + row[0];
        }
        return joined;
    }
};

}  // namespace bucu::test::cli
