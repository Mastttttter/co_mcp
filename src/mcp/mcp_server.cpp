#include "mcp_server.h"
#include <format>
#include <mutex>
#include <shared_mutex>
#include "logger.h"
#include "types.h"

namespace mcp {

void McpServer::RegisterTool(Tool const &tool, ToolHandler handler) {
  std::lock_guard lock(tools_mutex_);
  if (tools_.contains(tool.name)) {
    MCP_LOG_WARN("Tool '{}' already registered , overriding", tool.name);
  }
  tools_[tool.name] = tool;
  tool_handlers_[tool.name] = std::move(handler);
  MCP_LOG_INFO("tool registered {},{}", tool.name, tool.description);
  if (sse_callback_) {
    json event{{"type", "tools_changed"}, {"timestamp", std::time(nullptr)}};
    sse_callback_(event);
  }
}

std::vector<Tool> McpServer::ListTool() const {
  std::lock_guard lock(tools_mutex_);
  std::vector<Tool> result;
  for (auto const &[name, tool]: tools_) {
    result.push_back(tool);
  }
  return result;
}

async_simple::coro::Lazy<ToolResult> McpServer::CallTool(
    std::string const &name, json const &arguments) {
  std::shared_lock lock(tools_mutex_);
  auto handler_it = tool_handlers_.find(name);
  if (handler_it == tool_handlers_.end()) {
    ToolResult error{
        .content{
            {.type = "text", .text = std::format("Tools not found: {}", name)}},
        .type = ToolResultType::error};
    co_return error;
  }
  try {
    auto result = co_await handler_it->second(arguments);
    MCP_LOG_DEBUG("Tool {} executed successfully", name);
    co_return result;
  } catch (std::exception const &e) {
    ToolResult error{
        .content{{.type = "text",
                  .text = std::format("executing tool error: {}", name)}},
        .type = ToolResultType::error};
    co_return error;
  }
}

bool McpServer::HasTool(std::string const &name) const {
  return tools_.contains(name);
}

void McpServer::SetSseCallback(SseEventCallback callback) {
  sse_callback_ = callback;
}
}  // namespace mcp
