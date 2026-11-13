# The core module: domain rules with no SQLite or operating-system dependency (architecture
# section 4). Callers see only the headers in include/bucu/core/.
add_library(bucu_core STATIC utf8.cpp unicode_lower.cpp url.cpp)
target_include_directories(bucu_core
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_core)
target_link_libraries(bucu_core PRIVATE bucu::unicode)
