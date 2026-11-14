#pragma once

#include <filesystem>

namespace bucu::test {

// Damages the database at `path` the way the plan's trial did: 64 zero bytes at the start of the
// root page of the index on `bookmark.url`. The file still opens and reports its schema version,
// and `PRAGMA integrity_check` fails. Every connection to the file must be closed first. Throws
// std::runtime_error when the file cannot be damaged.
void damage_url_index(const std::filesystem::path& path);

}  // namespace bucu::test
