# SQLite amalgamation with FTS5, or the system library (BUCU_USE_SYSTEM_SQLITE).
# Both paths provide the target bucu::sqlite.
if(BUCU_USE_SYSTEM_SQLITE)
    find_package(SQLite3 REQUIRED)
    add_library(bucu_sqlite INTERFACE)
    target_link_libraries(bucu_sqlite INTERFACE SQLite::SQLite3)
else()
    find_package(Threads REQUIRED)
    add_library(bucu_sqlite STATIC sqlite3.c)
    target_include_directories(bucu_sqlite SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(bucu_sqlite PRIVATE SQLITE_ENABLE_FTS5)
    target_link_libraries(bucu_sqlite PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()
add_library(bucu::sqlite ALIAS bucu_sqlite)
