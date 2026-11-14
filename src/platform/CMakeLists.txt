# The platform module: data directory, environment, terminal, colour, and the UTF-8 console
# (architecture section 4). It depends on no other BuCu module; callers see only the headers in
# include/bucu/platform/.
add_library(bucu_platform STATIC paths.cpp)
target_include_directories(bucu_platform
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_platform)
