# CLI11, single header.
add_library(bucu_cli11 INTERFACE)
target_include_directories(bucu_cli11 SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(bucu::cli11 ALIAS bucu_cli11)
