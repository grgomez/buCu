add_executable(bucu_core_tests bookmark_id_test.cpp utf8_test.cpp unicode_lower_test.cpp word_characters_test.cpp url_test.cpp tags_test.cpp bookmark_test.cpp error_test.cpp id_selection_test.cpp changes_test.cpp search_test.cpp)
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
