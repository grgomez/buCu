# The db module: connection setup, migrations, prepared statements, and transactions
# (architecture section 4). Callers see only the headers in include/bucu/db/.
add_library(bucu_db STATIC bookmarks.cpp database.cpp statement.cpp transaction.cpp migrations.cpp)
target_include_directories(bucu_db
    PUBLIC "${PROJECT_SOURCE_DIR}/include"
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(bucu_db PRIVATE bucu::sqlite)
bucu_apply_warnings(bucu_db)
