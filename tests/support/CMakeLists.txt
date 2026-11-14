# Helpers shared by the db and cli tests (architecture section 9). Include them as
# "support/<name>.hpp".
add_library(bucu_test_support STATIC damage.cpp)
target_include_directories(bucu_test_support PUBLIC "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(bucu_test_support PUBLIC bucu::sqlite)
bucu_apply_warnings(bucu_test_support)
