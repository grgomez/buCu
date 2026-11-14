# The strict warning set of architecture section 8, applied to every BuCu target.
# Vendored sources (third_party/) never call this function.

# bucu_apply_warnings(<target>)
function(bucu_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /utf-8 /external:anglebrackets /external:W0)
        if(BUCU_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
            -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wcast-align
            -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} PRIVATE
                -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wuseless-cast)
        endif()
        if(BUCU_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
