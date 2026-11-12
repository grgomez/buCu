# Catch2 v3 amalgamated. catch_amalgamated.cpp also provides main().
add_library(bucu_catch2 STATIC catch_amalgamated.cpp)
target_include_directories(bucu_catch2 SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(bucu::catch2 ALIAS bucu_catch2)
