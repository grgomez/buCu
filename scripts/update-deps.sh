#!/bin/sh
# Download the vendored dependencies into third_party/ and verify each file's SHA256
# (architecture section 2). A hash mismatch stops the script before anything is written.
# To upgrade a dependency, change its version and hashes below, then run this script.
set -eu

root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

sqlite_version=3.54.1
sqlite_zip=sqlite-amalgamation-3540100.zip
sqlite_url=https://www.sqlite.org/2026/$sqlite_zip
sqlite_zip_sha=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9

catch2_version=3.16.1
catch2_base=https://github.com/catchorg/Catch2/releases/download/v$catch2_version
catch2_licence_url=https://raw.githubusercontent.com/catchorg/Catch2/v$catch2_version/LICENSE.txt
catch2_hpp_sha=f354576b9fe94f110dc4cc2210e6288c8d4915132f9ff97a57e61798eb9ad541
catch2_cpp_sha=51cbc6feeb16cdc8f37d757161526983409b4a82f96463f886e65528a2eff39b
catch2_licence_sha=c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566

cli11_version=2.7.2
cli11_url=https://github.com/CLIUtils/CLI11/releases/download/v$cli11_version/CLI11.hpp
cli11_licence_url=https://raw.githubusercontent.com/CLIUtils/CLI11/v$cli11_version/LICENSE
cli11_hpp_sha=ffa9a30da295c5858fb5f91f9f45771bab09471d7010a34c7c68c857a330dd76
cli11_licence_sha=cfc76368aef8f51868fa1cda6bcfa26e583406a4cf8e48e725e7624df24d8855

# fetch <url> <sha256> <name>: download to $tmp/<name> and verify the hash.
fetch() {
    curl -fsSL --retry 2 -o "$tmp/$3" "$1"
    actual=$(sha256sum "$tmp/$3" | cut -d ' ' -f 1)
    if [ "$actual" != "$2" ]; then
        echo "update-deps: SHA256 mismatch for $3 ($1)" >&2
        echo "  expected $2" >&2
        echo "  actual   $actual" >&2
        exit 1
    fi
}

fetch "$sqlite_url" "$sqlite_zip_sha" "$sqlite_zip"
fetch "$catch2_base/catch_amalgamated.hpp" "$catch2_hpp_sha" catch_amalgamated.hpp
fetch "$catch2_base/catch_amalgamated.cpp" "$catch2_cpp_sha" catch_amalgamated.cpp
fetch "$catch2_licence_url" "$catch2_licence_sha" catch2_LICENSE.txt
fetch "$cli11_url" "$cli11_hpp_sha" CLI11.hpp
fetch "$cli11_licence_url" "$cli11_licence_sha" cli11_LICENSE

# Every download verified; now write.
mkdir -p "$root/third_party/sqlite" "$root/third_party/catch2" "$root/third_party/cli11"

unzip -q -o -j "$tmp/$sqlite_zip" '*/sqlite3.c' '*/sqlite3.h' -d "$tmp/sqlite"
cp "$tmp/sqlite/sqlite3.c" "$tmp/sqlite/sqlite3.h" "$root/third_party/sqlite/"
cat > "$root/third_party/sqlite/LICENSE" <<LICENCE
SQLite is in the public domain. The authors have dedicated all copyright
interest in the source code to the public domain.
See https://www.sqlite.org/copyright.html
LICENCE
echo "$sqlite_version" > "$root/third_party/sqlite/VERSION"

cp "$tmp/catch_amalgamated.hpp" "$tmp/catch_amalgamated.cpp" "$root/third_party/catch2/"
cp "$tmp/catch2_LICENSE.txt" "$root/third_party/catch2/LICENSE.txt"
echo "$catch2_version" > "$root/third_party/catch2/VERSION"

cp "$tmp/CLI11.hpp" "$root/third_party/cli11/"
cp "$tmp/cli11_LICENSE" "$root/third_party/cli11/LICENSE"
echo "$cli11_version" > "$root/third_party/cli11/VERSION"

echo "update-deps: SQLite $sqlite_version, Catch2 $catch2_version, CLI11 $cli11_version verified"
