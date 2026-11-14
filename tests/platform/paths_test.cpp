#include "bucu/platform/paths.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include "catch_amalgamated.hpp"
#include "fake_environment.hpp"

namespace {

namespace fs = std::filesystem;

// A fresh directory under the system temporary directory, removed with the object (architecture
// section 9: tests never touch the home directory).
class TempDir {
public:
    TempDir() {
        std::random_device entropy;
        const std::uint64_t id = (static_cast<std::uint64_t>(entropy()) << 32U) | entropy();
        path_ = fs::temp_directory_path() / ("bucu-platform-test-" + std::to_string(id));
        fs::create_directories(path_);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    ~TempDir() {
        std::error_code ignored;
        fs::remove_all(path_, ignored);
    }

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

}  // namespace

TEST_CASE("a path built from UTF-8 text names the same file on every platform",
          "[platform][paths]") {
    const TempDir dir;
    const std::string name = "m\xC3\xBAsica-\xE6\x97\xA5\xE6\x9C\xAC.db";  // música-日本.db
    const fs::path file = dir.path() / bucu::platform::path_from_utf8(name);
    std::ofstream(file) << "x";

    std::vector<std::string> found;
    for (const auto& entry : fs::directory_iterator(dir.path())) {
        found.push_back(bucu::platform::utf8_from_path(entry.path().filename()));
    }
    REQUIRE(found.size() == 1);
    CHECK(found.front() == name);
    CHECK(fs::exists(
        bucu::platform::path_from_utf8(bucu::platform::utf8_from_path(dir.path()) + "/" + name)));
}

namespace {

using bucu::platform::Os;
using bucu::platform::resolve_database_location;
using bucu::platform::test::FakeEnvironment;

std::string default_path(Os os, const FakeEnvironment& environment) {
    const auto location = resolve_database_location(os, environment, std::nullopt);
    REQUIRE(location.has_value());
    CHECK(location->is_default);
    return location->path;
}

}  // namespace

TEST_CASE("the default location follows XDG_DATA_HOME when it is absolute (product section 3)",
          "[platform][paths]") {
    const FakeEnvironment environment({{"XDG_DATA_HOME", "/x/data"}, {"HOME", "/home/a"}});
    CHECK(default_path(Os::kLinuxOrBsd, environment) == "/x/data/bucu/bookmarks.db");
}

TEST_CASE("the default location falls back to HOME/.local/share", "[platform][paths]") {
    SECTION("XDG_DATA_HOME unset") {
        const FakeEnvironment environment({{"HOME", "/home/a"}});
        CHECK(default_path(Os::kLinuxOrBsd, environment) ==
              "/home/a/.local/share/bucu/bookmarks.db");
    }
    SECTION("XDG_DATA_HOME empty") {
        const FakeEnvironment environment({{"XDG_DATA_HOME", ""}, {"HOME", "/home/a"}});
        CHECK(default_path(Os::kLinuxOrBsd, environment) ==
              "/home/a/.local/share/bucu/bookmarks.db");
    }
    SECTION("XDG_DATA_HOME relative") {
        const FakeEnvironment environment({{"XDG_DATA_HOME", "data"}, {"HOME", "/home/a"}});
        CHECK(default_path(Os::kLinuxOrBsd, environment) ==
              "/home/a/.local/share/bucu/bookmarks.db");
    }
}

TEST_CASE("macOS uses Application Support and ignores XDG_DATA_HOME", "[platform][paths]") {
    const FakeEnvironment environment({{"XDG_DATA_HOME", "/x/data"}, {"HOME", "/Users/a"}});
    CHECK(default_path(Os::kMacOs, environment) ==
          "/Users/a/Library/Application Support/bucu/bookmarks.db");
}

TEST_CASE("Windows uses APPDATA, with non-ASCII text kept intact", "[platform][paths]") {
    const FakeEnvironment environment(
        {{"APPDATA", "C:\\Users\\Jos\xC3\xA9\\AppData\\Roaming"}, {"HOME", "/ignored"}});
    CHECK(default_path(Os::kWindows, environment) ==
          "C:\\Users\\Jos\xC3\xA9\\AppData\\Roaming\\bucu\\bookmarks.db");
}

TEST_CASE("a relative HOME or APPDATA is used as given", "[platform][paths]") {
    CHECK(default_path(Os::kLinuxOrBsd, FakeEnvironment({{"HOME", "home"}})) ==
          "home/.local/share/bucu/bookmarks.db");
    CHECK(default_path(Os::kMacOs, FakeEnvironment({{"HOME", "home"}})) ==
          "home/Library/Application Support/bucu/bookmarks.db");
    CHECK(default_path(Os::kWindows, FakeEnvironment({{"APPDATA", "roaming"}})) ==
          "roaming\\bucu\\bookmarks.db");
}

TEST_CASE("an explicit path wins over BUCU_DB, and BUCU_DB wins over the default",
          "[platform][paths]") {
    const FakeEnvironment environment({{"BUCU_DB", "/b.db"}, {"HOME", "/home/a"}});

    const auto explicit_wins = resolve_database_location(Os::kLinuxOrBsd, environment, "/a.db");
    REQUIRE(explicit_wins.has_value());
    CHECK(explicit_wins->path == "/a.db");
    CHECK_FALSE(explicit_wins->is_default);

    const auto env_wins = resolve_database_location(Os::kLinuxOrBsd, environment, std::nullopt);
    REQUIRE(env_wins.has_value());
    CHECK(env_wins->path == "/b.db");
    CHECK_FALSE(env_wins->is_default);
}

TEST_CASE("an empty explicit path or BUCU_DB counts as absent", "[platform][paths]") {
    const FakeEnvironment empty_env({{"BUCU_DB", ""}, {"HOME", "/home/a"}});

    const auto from_default = resolve_database_location(Os::kLinuxOrBsd, empty_env, "");
    REQUIRE(from_default.has_value());
    CHECK(from_default->is_default);

    const FakeEnvironment set_env({{"BUCU_DB", "/b.db"}, {"HOME", "/home/a"}});
    const auto from_env = resolve_database_location(Os::kLinuxOrBsd, set_env, "");
    REQUIRE(from_env.has_value());
    CHECK(from_env->path == "/b.db");
}

TEST_CASE("a relative override is kept as given", "[platform][paths]") {
    const FakeEnvironment environment({{"BUCU_DB", "b.db"}});
    const auto from_env = resolve_database_location(Os::kLinuxOrBsd, environment, std::nullopt);
    REQUIRE(from_env.has_value());
    CHECK(from_env->path == "b.db");

    const auto from_option = resolve_database_location(Os::kLinuxOrBsd, environment, "dir/a.db");
    REQUIRE(from_option.has_value());
    CHECK(from_option->path == "dir/a.db");
}
