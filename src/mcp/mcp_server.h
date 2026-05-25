#pragma once
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "async_simple/coro/Lazy.h"
#include "types.h"

namespace mcp {

class McpServer {
  public:
  McpServer();
  McpServer(std::string name, std::string version);

  using ToolHandler = std::function<async_simple::coro::Lazy<ToolResult>(
      json const &arguments)>;
  using SseEventCallback = std::function<void(json const &)>;

  InitializeResult GetInitializeResult() const;
  void RegisterTool(Tool const &tool, ToolHandler handler);
  std::vector<Tool> ListTool() const;
  async_simple::coro::Lazy<ToolResult> CallTool(std::string const &name,
                                                json const &arguments);
  bool HasTool(std::string const &name) const;

  void SetSseCallback(SseEventCallback callback);

  private:
  ServerInfo server_info_;
  std::unordered_map<std::string, Tool> tools_;
  std::unordered_map<std::string, ToolHandler> tool_handlers_;
  mutable std::shared_mutex tools_mutex_;

  SseEventCallback sse_callback_;
  mutable std::mutex sse_mutex_;
};
}  // namespace mcp
