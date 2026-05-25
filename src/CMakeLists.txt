add_library(mcp_lib STATIC
config/config.cpp
logger/logger.cpp
json_rpc/stdio_jsonrpc.cpp
json_rpc/http_jsonrpc.cpp
mcp/types.cpp
mcp/mcp_server.cpp
)

add_subdirectory(utilities)
target_include_directories(mcp_lib PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/config
${CMAKE_CURRENT_SOURCE_DIR}/logger
)

target_link_libraries(mcp_lib
Threads::Threads
nlohmann_json::nlohmann_json
spdlog::spdlog
)

target_compile_options(mcp_lib PUBLIC -freflection)

add_executable(mcp_server mcp_demo_server_main.cpp)
target_link_libraries(mcp_server mcp_lib)
add_custom_command(
  TARGET mcp_server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/resources
          $<TARGET_FILE_DIR:mcp_server>/resources
)
