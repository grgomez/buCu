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

# Unicode Character Database, for the tag lowercasing tables in core (plan decision D1 of #9).
unicode_version=18.0.0
unicode_base=https://www.unicode.org/Public/$unicode_version/ucd
unicode_licence_url=https://www.unicode.org/license.txt
unicode_data_sha=0736451de439ae7baf1425136617da495e09ee5afbe6e394374db7009ea08950
unicode_special_sha=8538dea57c184f1ef3783885ea79677b10f6efa06423717157e63712f14d1ad2
unicode_derived_sha=09c928886a178fcafd93c29e4bd59073a058e5a100b716d425cb563ab50f68c9
unicode_licence_sha=e7a93b009565cfce55919a381437ac4db883e9da2126fa28b91d12732bc53d96
# SHA256 of the generated files, so an awk that writes different bytes fails instead of
# changing the tables.
unicode_lowercase_sha=7d4d8cd4372eedac5242725e5ce5570579078910445128d37a69992f5a8ba8b8
unicode_properties_sha=ae6a5b7c555a30de22a35ddd4a7a4af8b16784e7fbd827b6d0d82608a78a2fc1

# sha256 <file>: print the SHA256 (GNU sha256sum, or shasum on macOS).
sha256() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | cut -d ' ' -f 1
    else
        shasum -a 256 "$1" | cut -d ' ' -f 1
    fi
}

# fetch <url> <sha256> <name>: download to $tmp/<name> and verify the hash.
fetch() {
    curl -fsSL --retry 2 -o "$tmp/$3" "$1"
    actual=$(sha256 "$tmp/$3")
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

fetch "$unicode_base/UnicodeData.txt" "$unicode_data_sha" UnicodeData.txt
fetch "$unicode_base/SpecialCasing.txt" "$unicode_special_sha" SpecialCasing.txt
fetch "$unicode_base/DerivedCoreProperties.txt" "$unicode_derived_sha" DerivedCoreProperties.txt
fetch "$unicode_licence_url" "$unicode_licence_sha" unicode_LICENSE

# hex <digits> is defined inside each awk program: POSIX awk has no strtonum.

# lowercase.inc: one "{code point, first, second}" row for each code point that has a simple
# lowercase mapping (UnicodeData.txt field 14), in ascending order. The unconditional
# SpecialCasing.txt entries whose lowercase differs replace the row; the second code point is 0
# when the mapping has one.
awk -F';' '
function hex(s,    i, n, c) {
    n = 0
    s = toupper(s)
    for (i = 1; i <= length(s); i++) {
        c = index("0123456789ABCDEF", substr(s, i, 1)) - 1
        n = n * 16 + c
    }
    return n
}
function trim(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
FNR == NR {
    sub(/#.*/, "")
    if (NF < 5) next
    if (trim($5) != "") next
    # A line with an empty condition list has 5 fields before the comment: code; lower; title; upper; (conditions)
    n = split(trim($2), a, " ")
    if (n == 2) special[hex(trim($1))] = hex(a[1]) " " hex(a[2])
    next
}
{
    if ($14 == "") next
    cp = hex($1)
    lo = hex($14)
    second = 0
    if (cp in special) {
        split(special[cp], b, " ")
        lo = b[1]
        second = b[2]
    }
    printf "{0x%04X, 0x%04X, 0x%04X},\n", cp, lo, second
}' "$tmp/SpecialCasing.txt" "$tmp/UnicodeData.txt" > "$tmp/lowercase.inc"

# case_properties.inc: the Cased and Case_Ignorable ranges of DerivedCoreProperties.txt, for the
# Final_Sigma rule, in ascending order.
awk '
function hex(s,    i, n, c) {
    n = 0
    s = toupper(s)
    for (i = 1; i <= length(s); i++) {
        c = index("0123456789ABCDEF", substr(s, i, 1)) - 1
        n = n * 16 + c
    }
    return n
}
function trim(s) { gsub(/^[ \t]+|[ \t]+$/, "", s); return s }
function emit(prop, name,    i) {
    printf "inline constexpr CodePointRange %s[] = {\n", name
    for (i = 1; i <= count[prop]; i++) printf "    %s\n", rows[prop, i]
    printf "};\n"
}
{
    line = $0
    sub(/#.*/, "", line)
    n = split(line, f, ";")
    if (n < 2) next
    prop = trim(f[2])
    if (prop != "Cased" && prop != "Case_Ignorable") next
    range = trim(f[1])
    if (index(range, "..") > 0) {
        lo = hex(substr(range, 1, index(range, "..") - 1))
        hi = hex(substr(range, index(range, "..") + 2))
    } else {
        lo = hex(range)
        hi = lo
    }
    count[prop]++
    rows[prop, count[prop]] = sprintf("{0x%04X, 0x%04X},", lo, hi)
}
END {
    emit("Cased", "kCased")
    emit("Case_Ignorable", "kCaseIgnorable")
}' "$tmp/DerivedCoreProperties.txt" > "$tmp/case_properties.inc"

check_generated() {
    actual=$(sha256 "$tmp/$1")
    if [ "$actual" != "$2" ]; then
        echo "update-deps: SHA256 mismatch for generated $1" >&2
        echo "  expected $2" >&2
        echo "  actual   $actual" >&2
        exit 1
    fi
}
check_generated lowercase.inc "$unicode_lowercase_sha"
check_generated case_properties.inc "$unicode_properties_sha"

# Every download verified and every generated file checked; now write.
mkdir -p "$root/third_party/sqlite" "$root/third_party/catch2" "$root/third_party/cli11" \
    "$root/third_party/unicode"

unzip -q -o -j "$tmp/$sqlite_zip" '*/sqlite3.c' '*/sqlite3.h' -d "$tmp/sqlite"
cp "$tmp/sqlite/sqlite3.c" "$tmp/sqlite/sqlite3.h" "$root/third_party/sqlite/"
cat > "$root/third_party/sqlite/LICENSE" <<LICENCE
SQLite is in the public domain. The authors have dedicated all copyright
interest in the source code to the public domain.
See https://www.sqlite.org/copyright.html
LICENCE
echo "$sqlite_version" > "$root/third_party/sqlite/VERSION.txt"

cp "$tmp/catch_amalgamated.hpp" "$tmp/catch_amalgamated.cpp" "$root/third_party/catch2/"
cp "$tmp/catch2_LICENSE.txt" "$root/third_party/catch2/LICENSE.txt"
echo "$catch2_version" > "$root/third_party/catch2/VERSION.txt"

cp "$tmp/CLI11.hpp" "$root/third_party/cli11/"
cp "$tmp/cli11_LICENSE" "$root/third_party/cli11/LICENSE"
echo "$cli11_version" > "$root/third_party/cli11/VERSION.txt"

cp "$tmp/lowercase.inc" "$tmp/case_properties.inc" "$root/third_party/unicode/"
cp "$tmp/unicode_LICENSE" "$root/third_party/unicode/LICENSE"
echo "$unicode_version" > "$root/third_party/unicode/VERSION.txt"

echo "update-deps: SQLite $sqlite_version, Catch2 $catch2_version, CLI11 $cli11_version," \
    "Unicode $unicode_version verified"
