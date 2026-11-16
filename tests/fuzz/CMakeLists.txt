# libFuzzer targets for the core parsers (ADR 0010). Added by tests/CMakeLists.txt when BUCU_FUZZ is
# on. ctest replays each seed corpus with -runs=0 as fuzz-<name>, so a seed that once crashed stays
# a regression test.

# bucu_fuzz_target(<name>): the target <name>_fuzz from <name>_fuzz.cpp and its ctest replay.
function(bucu_fuzz_target name)
    add_executable(${name}_fuzz ${name}_fuzz.cpp)
    target_link_libraries(${name}_fuzz PRIVATE bucu_core)
    target_include_directories(${name}_fuzz PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_options(${name}_fuzz PRIVATE -fsanitize=fuzzer)
    bucu_apply_warnings(${name}_fuzz)
    add_test(NAME fuzz-${name}
        COMMAND ${name}_fuzz -runs=0 "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}")
endfunction()

bucu_fuzz_target(utf8)
