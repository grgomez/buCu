# The platform module: data directory, environment, terminal, colour, and the UTF-8 console
# (architecture section 4). It depends on no other BuCu module; callers see only the headers in
# include/bucu/platform/.
add_library(bucu_platform STATIC paths.cpp environment.cpp terminal.cpp console.cpp)
target_include_directories(bucu_platform
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_platform)

# Operating-system calls for tests (architecture section 9). Linked only into the test executable.
add_library(bucu_platform_test_support STATIC test_support.cpp)
target_include_directories(bucu_platform_test_support PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_platform_test_support)
