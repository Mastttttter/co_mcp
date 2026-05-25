include(GoogleTest)

add_executable(test_config
  test_config.cpp
)
target_link_libraries(test_config
  mcp_lib
  GTest::gtest
)
gtest_discover_tests(test_config)


add_executable(test_logger
test_logger.cpp
)
target_link_libraries(test_logger
  mcp_lib
  GTest::gtest
)
gtest_discover_tests(test_logger)

add_executable(test_mcp_server
  test_mcp_server.cpp
)
target_link_libraries(test_mcp_server
  mcp_lib
  GTest::gtest
)
gtest_discover_tests(test_mcp_server)


add_custom_command(
  TARGET test_config POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/resources
          $<TARGET_FILE_DIR:test_config>/resources
)
