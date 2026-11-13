#pragma once

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace bucu::db::test {

// A fresh directory under the system temporary directory, removed with the object (architecture
// section 9: tests are independent and never touch the home directory).
class TempDir {
public:
    TempDir() {
        std::random_device entropy;
        const std::uint64_t id = (static_cast<std::uint64_t>(entropy()) << 32U) | entropy();
        path_ = std::filesystem::temp_directory_path() / ("bucu-db-test-" + std::to_string(id));
        std::filesystem::create_directories(path_);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

// The path as UTF-8 in a std::string, the way Database::open hands it to SQLite.
inline std::string utf8_path(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

}  // namespace bucu::db::test
