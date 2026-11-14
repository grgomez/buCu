add_executable(bucu_cli_tests record_test.cpp run_test.cpp)
target_link_libraries(bucu_cli_tests PRIVATE bucu_cli bucu_core bucu_platform bucu_test_support bucu::catch2)
target_compile_definitions(bucu_cli_tests PRIVATE
    BUCU_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
target_include_directories(bucu_cli_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_cli_tests)
add_test(NAME cli COMMAND bucu_cli_tests)
