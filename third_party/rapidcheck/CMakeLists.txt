# RapidCheck, pinned commit (see VERSION.txt). Our own build file: the upstream one forces C++11
# and changes the global flags. Used by tests only; core never links it.
file(GLOB_RECURSE bucu_rapidcheck_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_library(bucu_rapidcheck STATIC ${bucu_rapidcheck_sources})
target_include_directories(bucu_rapidcheck SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/extras/catch/include")
if(MSVC)
    target_compile_options(bucu_rapidcheck PRIVATE /bigobj)
endif()
add_library(bucu::rapidcheck ALIAS bucu_rapidcheck)
