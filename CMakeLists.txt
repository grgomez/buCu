cmake_minimum_required(VERSION 3.25)

project(bucu
    VERSION 0.1.0
    DESCRIPTION "A command-line bookmark manager, a C++ port of Buku"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Architecture section 8: warnings are errors in every preset and in CI, but not by default,
# so a packager's newer compiler does not break the build.
option(BUCU_WERROR "Treat compiler warnings as errors" OFF)
# Architecture section 2: link the system SQLite instead of the vendored amalgamation.
option(BUCU_USE_SYSTEM_SQLITE "Link the system SQLite library instead of third_party/sqlite" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(BucuWarnings)

add_subdirectory(third_party)

# Windows: the executable runs with the UTF-8 active code page (constitution principle 12).
# The bucu target adds this file to its sources.
if(WIN32)
    set(BUCU_WINDOWS_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows/bucu.manifest")
endif()

enable_testing()
add_subdirectory(src/cli)
add_subdirectory(src/db)
add_subdirectory(tests)

add_executable(bucu src/cli/main.cpp ${BUCU_WINDOWS_MANIFEST})
target_link_libraries(bucu PRIVATE bucu_cli)
target_include_directories(bucu PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu)
