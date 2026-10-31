add_executable(api_test
  cplusplus.cpp
  harness.c
  harness.h
  main.c
)
include_directories(
  ${PROJECT_SOURCE_DIR}/src/include
  ${PROJECT_BINARY_DIR}/src
  ${PROJECT_SOURCE_DIR}/extensions/include
  ${PROJECT_BINARY_DIR}/extensions
)
target_link_libraries(api_test PRIVATE
  libcmark-gfm
  libcmark-gfm-extensions)

add_test(NAME api_test COMMAND api_test)

add_executable(web_table_test web_table.c harness.c)
target_link_libraries(web_table_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_table_test COMMAND web_table_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(api_test web_table_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_autolink_closeout_test web_autolink_closeout.c harness.c)
target_link_libraries(web_autolink_closeout_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_autolink_closeout_test COMMAND web_autolink_closeout_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_autolink_closeout_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_table_prefix_test web_table_prefix.c harness.c)
target_link_libraries(web_table_prefix_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_table_prefix_test COMMAND web_table_prefix_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_table_prefix_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_inline_html_closeout_test web_inline_html_closeout.c harness.c)
target_link_libraries(web_inline_html_closeout_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_inline_html_closeout_test COMMAND web_inline_html_closeout_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_inline_html_closeout_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_tilde_positions_test web_tilde_positions.c harness.c)
target_link_libraries(web_tilde_positions_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_tilde_positions_test COMMAND web_tilde_positions_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_tilde_positions_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_list_boundaries_test web_list_boundaries.c harness.c)
target_link_libraries(web_list_boundaries_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_list_boundaries_test COMMAND web_list_boundaries_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_list_boundaries_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_definition_breaks_test web_definition_breaks.c harness.c)
target_link_libraries(web_definition_breaks_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_definition_breaks_test COMMAND web_definition_breaks_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_definition_breaks_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()

add_executable(web_html_blocks_test web_html_blocks.c harness.c)
target_link_libraries(web_html_blocks_test PRIVATE libcmark-gfm libcmark-gfm-extensions)
add_test(NAME web_html_blocks_test COMMAND web_html_blocks_test)
if(WIN32 AND BUILD_SHARED_LIBS)
  set_tests_properties(web_html_blocks_test PROPERTIES
    ENVIRONMENT "PATH=$<TARGET_FILE_DIR:libcmark-gfm>;$<TARGET_FILE_DIR:libcmark-gfm-extensions>;$ENV{PATH}")
endif()
