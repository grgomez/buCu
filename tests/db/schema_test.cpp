#include <cstdint>
#include <expected>
#include <string>
#include <utility>

#include "bucu/db/database.hpp"
#include "catch_amalgamated.hpp"
#include "db/handle.hpp"
#include "db/statement.hpp"
#include "temp_dir.hpp"

namespace {

using bucu::db::Database;
using bucu::db::Statement;
using bucu::db::test::TempDir;

// A version 1 database with bookmarks 1 and 2, tags 1 and 2, and the links (1,1), (1,2), (2,1).
struct Fixture {
    TempDir dir;
    Database database = open_database();

    Fixture() {
        run("INSERT INTO bookmark (id, url, created_at, updated_at) VALUES "
            "(1, 'https://one.example/', 0, 0), (2, 'https://two.example/', 0, 0)");
        run("INSERT INTO tag (id, name) VALUES (1, 'a'), (2, 'b')");
        run("INSERT INTO bookmark_tag (bookmark_id, tag_id) VALUES (1, 1), (1, 2), (2, 1)");
    }

    std::expected<bool, bucu::db::Error> try_run(const char* sql) {
        auto statement = Statement::prepare(bucu::db::detail::handle(database), sql);
        REQUIRE(statement.has_value());
        return statement->step();
    }

    void run(const char* sql) { REQUIRE(try_run(sql).has_value()); }

    std::int64_t count(const char* table) {
        auto statement = Statement::prepare(bucu::db::detail::handle(database),
                                            (std::string("SELECT count(*) FROM ") + table).c_str());
        REQUIRE(statement.has_value());
        REQUIRE(statement->step().value());
        return statement->column_int(0);
    }

private:
    Database open_database() const {
        auto opened = Database::open(dir.path() / "bookmarks.db");
        REQUIRE(opened.has_value());
        return std::move(*opened);
    }
};

}  // namespace

TEST_CASE("deleting a bookmark deletes its links and keeps the tags", "[db]") {
    Fixture fixture;

    fixture.run("DELETE FROM bookmark WHERE id = 1");

    CHECK(fixture.count("bookmark_tag") == 1);
    CHECK(fixture.count("tag") == 2);
    CHECK(fixture.count("bookmark") == 1);
}

TEST_CASE("deleting a tag deletes its links and keeps the bookmarks", "[db]") {
    Fixture fixture;

    fixture.run("DELETE FROM tag WHERE id = 1");

    CHECK(fixture.count("bookmark_tag") == 1);
    CHECK(fixture.count("bookmark") == 2);
    CHECK(fixture.count("tag") == 1);
}

TEST_CASE("a link to a missing bookmark or tag is rejected and changes nothing", "[db]") {
    Fixture fixture;

    CHECK_FALSE(fixture.try_run("INSERT INTO bookmark_tag VALUES (99, 1)").has_value());
    CHECK_FALSE(fixture.try_run("INSERT INTO bookmark_tag VALUES (1, 99)").has_value());

    CHECK(fixture.count("bookmark_tag") == 3);
}

TEST_CASE("a duplicate bookmark URL or tag name is rejected and changes nothing", "[db]") {
    Fixture fixture;

    CHECK_FALSE(fixture
                    .try_run("INSERT INTO bookmark (url, created_at, updated_at) "
                             "VALUES ('https://one.example/', 0, 0)")
                    .has_value());
    CHECK_FALSE(fixture.try_run("INSERT INTO tag (name) VALUES ('a')").has_value());

    CHECK(fixture.count("bookmark") == 2);
    CHECK(fixture.count("tag") == 2);
}

TEST_CASE("version 1 has the tag-to-bookmark index on bookmark_tag(tag_id)", "[db]") {
    Fixture fixture;

    auto statement = Statement::prepare(
        bucu::db::detail::handle(fixture.database),
        "SELECT count(*) FROM pragma_index_info('bookmark_tag_tag_id') WHERE name = 'tag_id'");
    REQUIRE(statement.has_value());
    REQUIRE(statement->step().value());

    CHECK(statement->column_int(0) == 1);
}
