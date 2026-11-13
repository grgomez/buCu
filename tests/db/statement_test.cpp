#include "db/statement.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <string>
#include <utility>

#include "bucu/db/database.hpp"
#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "db/transaction.hpp"
#include "temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::db::ErrorCode;
using bucu::db::Statement;
using bucu::db::Transaction;
using bucu::db::test::TempDir;

// An open database with a scratch table `item(id INTEGER PRIMARY KEY, name TEXT UNIQUE, note)`.
struct Fixture {
    TempDir dir;
    Database database = open_database();
    sqlite3* connection = bucu::db::detail::handle(database);

    Fixture() { run("CREATE TABLE item (id INTEGER PRIMARY KEY, name TEXT UNIQUE, note TEXT)"); }

    void run(const char* sql) const {
        auto statement = Statement::prepare(connection, sql);
        REQUIRE(statement.has_value());
        REQUIRE(statement->step().has_value());
    }

    std::int64_t count() const {
        auto statement = Statement::prepare(connection, "SELECT count(*) FROM item");
        REQUIRE(statement.has_value());
        REQUIRE(statement->step().value());
        return statement->column_int(0);
    }

private:
    Database open_database() const {
        auto opened = Database::open(dir.path() / "scratch.db");
        REQUIRE(opened.has_value());
        return std::move(*opened);
    }
};

}  // namespace

TEST_CASE("a statement binds values, steps rows, and reads columns", "[db]") {
    Fixture fixture;
    auto insert =
        Statement::prepare(fixture.connection, "INSERT INTO item (name, note) VALUES (?1, ?2)");
    REQUIRE(insert.has_value());
    REQUIRE(insert->bind(1, "caf\xC3\xA9").has_value());
    REQUIRE(insert->bind(2, std::int64_t{42}).has_value());
    REQUIRE(insert->step().has_value());

    auto select = Statement::prepare(fixture.connection, "SELECT id, name, note FROM item");
    REQUIRE(select.has_value());
    const auto row = select->step();
    REQUIRE(row.has_value());
    REQUIRE(*row);
    CHECK(select->column_int(0) == 1);
    CHECK(select->column_text(1) == "caf\xC3\xA9");
    CHECK(select->column_int(2) == 42);
    const auto end = select->step();
    REQUIRE(end.has_value());
    CHECK_FALSE(*end);
}

TEST_CASE("a statement can be reset and run again with new values", "[db]") {
    Fixture fixture;
    auto insert = Statement::prepare(fixture.connection, "INSERT INTO item (name) VALUES (?1)");
    REQUIRE(insert.has_value());
    for (const char* name : {"one", "two"}) {
        REQUIRE(insert->bind(1, name).has_value());
        REQUIRE(insert->step().has_value());
        REQUIRE(insert->reset().has_value());
    }
    CHECK(fixture.count() == 2);
}

TEST_CASE("preparing bad SQL returns an error with SQLite's message", "[db]") {
    Fixture fixture;

    auto statement = Statement::prepare(fixture.connection, "SELEC nothing");

    REQUIRE_FALSE(statement.has_value());
    CHECK(statement.error().code == ErrorCode::kSql);
    CHECK(statement.error().message.find("syntax error") != std::string::npos);
}

TEST_CASE("a failing step returns an error with SQLite's message", "[db]") {
    Fixture fixture;
    fixture.run("INSERT INTO item (name) VALUES ('same')");
    auto insert = Statement::prepare(fixture.connection, "INSERT INTO item (name) VALUES ('same')");
    REQUIRE(insert.has_value());

    const auto result = insert->step();

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ErrorCode::kSql);
    CHECK(result.error().message.find("UNIQUE") != std::string::npos);
}

TEST_CASE("a transaction commits its changes", "[db]") {
    Fixture fixture;
    {
        auto transaction = Transaction::begin(fixture.connection);
        REQUIRE(transaction.has_value());
        fixture.run("INSERT INTO item (name) VALUES ('kept')");
        REQUIRE(transaction->commit().has_value());
    }
    CHECK(fixture.count() == 1);
}

TEST_CASE("a transaction that is not committed rolls back", "[db]") {
    Fixture fixture;
    {
        auto transaction = Transaction::begin(fixture.connection);
        REQUIRE(transaction.has_value());
        fixture.run("INSERT INTO item (name) VALUES ('dropped')");
    }
    CHECK(fixture.count() == 0);
}
