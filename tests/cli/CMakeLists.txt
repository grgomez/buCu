add_executable(bucu_cli_tests add_test.cpp db_test.cpp record_test.cpp run_test.cpp)
target_link_libraries(bucu_cli_tests PRIVATE bucu_cli bucu_core bucu_platform bucu_test_support bucu::sqlite bucu::catch2)
target_compile_definitions(bucu_cli_tests PRIVATE
    BUCU_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
target_include_directories(bucu_cli_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_cli_tests)
add_test(NAME cli COMMAND bucu_cli_tests)

# Runs the built program with redirected output and fails on a CR byte (a Windows-only risk, so
# only the Windows job can see it fail).
add_test(NAME cli_lf_output
    COMMAND "${CMAKE_COMMAND}"
        "-DBUCU_EXE=$<TARGET_FILE:bucu>"
        "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/lf_output"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/lf_output.cmake")
