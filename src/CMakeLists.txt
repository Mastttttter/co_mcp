add_library(mcp_lib STATIC
config/config.cpp
logger/logger.cpp
json_rpc/stdio_jsonrpc.cpp
json_rpc/jsonrpc_serialization.cpp
)

target_include_directories(mcp_lib PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/config
${CMAKE_CURRENT_SOURCE_DIR}/utilities
${CMAKE_CURRENT_SOURCE_DIR}/logger
)

target_link_libraries(mcp_lib
Threads::Threads
nlohmann_json::nlohmann_json
spdlog::spdlog
)

target_compile_options(mcp_lib PUBLIC -freflection)
