add_executable(bucu_cli_tests run_test.cpp)
target_link_libraries(bucu_cli_tests PRIVATE bucu_cli bucu::catch2)
target_compile_definitions(bucu_cli_tests PRIVATE
    BUCU_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
bucu_apply_warnings(bucu_cli_tests)
add_test(NAME cli COMMAND bucu_cli_tests)
