#include "bucu/platform/paths.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include "catch_amalgamated.hpp"

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
