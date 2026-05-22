add_library(mcp_lib STATIC
config/config.cpp
)

target_include_directories(mcp_lib PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/config
)

target_link_libraries(mcp_lib
Threads::Threads
nlohmann_json::nlohmann_json
spdlog::spdlog
)
target_include_directories(mcp_lib PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}/config
${CMAKE_CURRENT_SOURCE_DIR}/utilities
)

target_compile_options(mcp_lib PUBLIC -freflection)
