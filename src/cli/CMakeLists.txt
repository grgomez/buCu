# The cli module: argument parsing, output, and exit codes (architecture section 4).
add_library(bucu_cli STATIC run.cpp)
target_include_directories(bucu_cli PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(bucu_cli PRIVATE BUCU_VERSION="${PROJECT_VERSION}")
target_link_libraries(bucu_cli PRIVATE bucu::cli11)
bucu_apply_warnings(bucu_cli)
