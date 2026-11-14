# Helpers shared by the db and cli tests (architecture section 9). Include them as
# "support/<name>.hpp".
add_library(bucu_test_support INTERFACE)
target_include_directories(bucu_test_support INTERFACE "${PROJECT_SOURCE_DIR}/tests")
