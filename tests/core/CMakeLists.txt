add_executable(bucu_core_tests bookmark_id_test.cpp utf8_test.cpp unicode_lower_test.cpp url_test.cpp tags_test.cpp bookmark_test.cpp error_test.cpp)
target_link_libraries(bucu_core_tests PRIVATE bucu_core bucu::catch2)
target_include_directories(bucu_core_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_core_tests)
add_test(NAME core COMMAND bucu_core_tests)

# Property tests (ADR 0009): RapidCheck properties for the rules of product section 4. ctest runs
# them with a fixed seed so CI is repeatable; run the binary with another RC_PARAMS to explore
# other seeds (architecture section 9).
add_executable(bucu_core_property_tests utf8_property_test.cpp unicode_lower_property_test.cpp tags_property_test.cpp url_property_test.cpp bookmark_property_test.cpp)
target_link_libraries(bucu_core_property_tests PRIVATE bucu_core bucu::catch2 bucu::rapidcheck bucu::unicode)
target_include_directories(bucu_core_property_tests PRIVATE "${PROJECT_SOURCE_DIR}/src")
bucu_apply_warnings(bucu_core_property_tests)
add_test(NAME core-property COMMAND bucu_core_property_tests)
set_tests_properties(core-property PROPERTIES ENVIRONMENT "RC_PARAMS=seed=1 max_success=100")

# A call of make_bookmark that leaves out the URL, title, or description must not build (FR-003).
# Each entry runs the compiler directly, with its own flags so BUCU_WERROR does not matter, on a
# file that is no target (so clang-tidy never analyses it), and passes only when the output names
# the missing field. MSVC is not covered.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  foreach(field url title description)
    string(TOUPPER "${field}" field_macro)
    add_test(NAME core-bookmark-fields-required-${field}
             COMMAND "${CMAKE_CXX_COMPILER}" -std=c++23 -Wextra -Werror -fsyntax-only
                     -I${PROJECT_SOURCE_DIR}/include -DOMIT_${field_macro}
                     "${CMAKE_CURRENT_SOURCE_DIR}/bookmark_fields_required.cpp")
    set_tests_properties(core-bookmark-fields-required-${field} PROPERTIES
                         PASS_REGULAR_EXPRESSION "missing.*${field}")
  endforeach()
endif()
