#!/bin/sh
# Format the C++ files of BuCu with clang-format. With --check, change nothing and fail when a
# file needs formatting. Vendored code in third_party/ is never touched.
set -eu

mode=-i
case "${1:-}" in
    --check) mode="--dry-run --Werror" ;;
    "") ;;
    *)
        echo "usage: scripts/format.sh [--check]" >&2
        exit 2
        ;;
esac

cd "$(dirname "$0")/.."
if ! command -v clang-format >/dev/null 2>&1; then
    echo "format.sh: clang-format is not installed." >&2
    exit 1
fi

# shellcheck disable=SC2086
git ls-files -z -- 'include/*' 'src/*' 'tests/*' 'benchmarks/*' |
    grep -zE '\.(cc|cpp|cxx|h|hh|hpp|hxx)$' |
    xargs -0 -r clang-format $mode
