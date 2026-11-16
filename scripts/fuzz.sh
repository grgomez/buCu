#!/bin/sh
# Fuzz the core parsers with libFuzzer (architecture section 9, ADR 0010). Configures and builds the
# fuzz preset, replays the seed corpus through ctest, then fuzzes each target for the given time.
# Each target starts from the seeds in tests/fuzz/corpus/<target>/ and keeps its new inputs in a
# working corpus under build/fuzz/corpus/, so the repository is never changed. A finding leaves its
# input in build/fuzz/artifacts/ and makes the script exit non-zero.
#
# usage: scripts/fuzz.sh [seconds]   (seconds per target, default 60)
# CC and CXX name the compilers (default clang-19 and clang++-19; libFuzzer needs LLVM Clang).
set -eu

if [ "$#" -gt 1 ]; then
    echo "usage: scripts/fuzz.sh [seconds]" >&2
    exit 2
fi
seconds=${1:-60}
case "$seconds" in
    "" | *[!0-9]*)
        echo "fuzz.sh: seconds must be a whole number, not '$seconds'." >&2
        exit 2
        ;;
esac

cd "$(dirname "$0")/.."
cc=${CC:-clang-19}
cxx=${CXX:-clang++-19}
if ! command -v "$cxx" >/dev/null 2>&1; then
    echo "fuzz.sh: $cxx is not installed; libFuzzer needs LLVM Clang (set CC and CXX)." >&2
    exit 1
fi

cmake --preset fuzz -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx"
cmake --build --preset fuzz
ctest --preset fuzz

build=build/fuzz
mkdir -p "$build/artifacts"
status=0
for target in utf8 url tags; do
    corpus="$build/corpus/$target"
    mkdir -p "$corpus"
    echo "fuzz.sh: fuzzing $target for $seconds s"
    if ! "$build/tests/fuzz/${target}_fuzz" -max_total_time="$seconds" \
        -artifact_prefix="$build/artifacts/$target-" "$corpus" "tests/fuzz/corpus/$target"; then
        echo "fuzz.sh: $target failed; the input is in $build/artifacts/." >&2
        status=1
    fi
done
exit "$status"
