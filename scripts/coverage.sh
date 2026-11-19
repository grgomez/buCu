#!/bin/sh
# Coverage and CRAP report for BuCu (ADR 0011). Merges the profiles that `ctest --preset
# coverage` leaves in <build-dir>/profiles, writes the llvm-cov line, region, and branch report,
# lcov and JSON exports, an HTML report, lizard complexity, and crap.md (functions by CRAP score).
# Only src/ and include/ are reported; third_party/ and tests/ are not.
#
# usage: scripts/coverage.sh [build-dir]   (default build/coverage; run the coverage tests first)
# Output goes to <build-dir>/report. LLVM_COV, LLVM_PROFDATA, and LIZARD name the tools
# (LLVM 19 or newer; lizard 1.26.1, `uv tool install lizard==1.26.1`).
set -u

if [ "$#" -gt 1 ]; then
    echo "usage: scripts/coverage.sh [build-dir]" >&2
    exit 2
fi

cd "$(dirname "$0")/.."
root=$(pwd)
build=${1:-build/coverage}
llvm_cov=${LLVM_COV:-llvm-cov}
llvm_profdata=${LLVM_PROFDATA:-llvm-profdata}
lizard=${LIZARD:-lizard}

for tool in "$llvm_cov" "$llvm_profdata" "$lizard"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "coverage.sh: $tool is not installed (set LLVM_COV, LLVM_PROFDATA, or LIZARD)." >&2
        exit 2
    fi
done

profiles=$(find "$build/profiles" -name '*.profraw' 2>/dev/null)
if [ -z "$profiles" ]; then
    echo "coverage.sh: no .profraw files in $build/profiles; run ctest --preset coverage first." >&2
    exit 2
fi

report=$build/report
rm -rf "$report"
mkdir -p "$report"

# shellcheck disable=SC2086
"$llvm_profdata" merge -sparse $profiles -o "$report/all.profdata" || exit 1

# The bucu binary is the primary object; every test executable adds its own counters.
primary=$build/bucu
objects=""
for exe in $(find "$build/tests" -type f -perm -u+x -name 'bucu_*' | sort); do
    objects="$objects -object $exe"
done
sources=$(find "$root/src" "$root/include" -type f)

cov() {
    command=$1
    shift
    # shellcheck disable=SC2086
    "$llvm_cov" "$command" "$primary" $objects -instr-profile="$report/all.profdata" "$@" $sources
}

cov report >"$report/coverage.txt" || exit 1
cov export -format=lcov >"$report/coverage.lcov" || exit 1
cov export -format=text >"$report/coverage.json" || exit 1
cov show -format=html -output-dir="$report/html" >/dev/null || exit 1
"$lizard" --csv src include >"$report/lizard.csv" || exit 1
python3 scripts/crap.py --lizard "$report/lizard.csv" --coverage "$report/coverage.json" \
    --root "$root" --out "$report/crap.md" || exit 1

echo "coverage.sh: report in $report"
tail -n 1 "$report/coverage.txt"
