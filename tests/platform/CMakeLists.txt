add_executable(bucu_platform_tests paths_test.cpp environment_test.cpp)
target_link_libraries(bucu_platform_tests PRIVATE bucu_platform bucu_platform_test_support
    bucu::catch2)
target_include_directories(bucu_platform_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_platform_tests)
add_test(NAME platform COMMAND bucu_platform_tests)
