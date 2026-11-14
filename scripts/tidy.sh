#!/bin/sh
# Static analysis for BuCu (architecture section 8, ADR 0006). Runs clang-tidy over every src/
# and tests/ translation unit, then two text checks: a NOLINT must name its check, and no BuCu
# code throws (architecture section 7). Third-party code is never analysed.
#
# usage: scripts/tidy.sh [build-dir]   (default build/debug; it needs compile_commands.json)
# CLANG_TIDY names the clang-tidy binary (version 19 or newer: 18 cannot parse std::expected).
set -u

if [ "$#" -gt 1 ]; then
    echo "usage: scripts/tidy.sh [build-dir]" >&2
    exit 2
fi

cd "$(dirname "$0")/.."
root=$(pwd)
build=${1:-build/debug}
tidy=${CLANG_TIDY:-clang-tidy}
status=0

if [ ! -f "$build/compile_commands.json" ]; then
    echo "tidy.sh: $build/compile_commands.json not found; configure the build first." >&2
    exit 2
fi
if ! command -v "$tidy" >/dev/null 2>&1; then
    echo "tidy.sh: $tidy is not installed (set CLANG_TIDY)." >&2
    exit 2
fi
if ! command -v run-clang-tidy >/dev/null 2>&1; then
    echo "tidy.sh: run-clang-tidy is not installed." >&2
    exit 2
fi

echo "tidy.sh: clang-tidy over src/ and tests/"
files=$(git ls-files -- 'src/*.cpp' 'tests/*.cpp' | sed "s|^|$root/|")
# shellcheck disable=SC2086
if ! run-clang-tidy -quiet -clang-tidy-binary "$tidy" -p "$build" $files; then
    status=1
fi

echo "tidy.sh: NOLINT must name its check"
# Fails on a NOLINT with no list, an empty list, or a wildcard list, and on a bare NOLINTEND.
nolint_bad='NOLINT(NEXTLINE|BEGIN|END)?([^(A-Za-z]|$)|NOLINT(NEXTLINE|BEGIN|END)?\(([^)]*\*[^)]*|[[:space:]]*)\)'
if git grep -nE "$nolint_bad" -- 'src' 'tests' 'include'; then
    echo "tidy.sh: a NOLINT above names no check; write NOLINT(<check>) and a reason." >&2
    status=1
fi

echo "tidy.sh: no throw in src/ or tests/, no catch outside src/cli/"
if git grep -nE '(^|[^A-Za-z0-9_])throw([^A-Za-z0-9_]|$)' -- 'src' 'tests' 'include' |
    grep -vE '^[^:]+:[0-9]+:[[:space:]]*//'; then
    echo "tidy.sh: a throw above; BuCu code returns an error (architecture section 7)." >&2
    status=1
fi
if git grep -nE '(^|[^A-Za-z0-9_])catch[[:space:]]*\(' -- 'src' 'tests' 'include' ':!src/cli' |
    grep -vE '^[^:]+:[0-9]+:[[:space:]]*//'; then
    echo "tidy.sh: a catch above; only src/cli/ catches, around third-party code that throws." >&2
    status=1
fi

exit $status
