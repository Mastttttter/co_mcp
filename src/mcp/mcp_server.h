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
  using ResourceProvider =
      std::function<async_simple::coro::Lazy<ResourceContent>(
          std::string const &uri)>;
  using SseEventCallback = std::function<void(json const &)>;

  InitializeResult GetInitializeResult() const;
  void RegisterTool(Tool const &tool, ToolHandler handler);
  std::vector<Tool> ListTool() const;
  async_simple::coro::Lazy<ToolResult> CallTool(std::string const &name,
                                                json const &arguments);
  bool HasTool(std::string const &name) const;

  void RegisterResource(Resource const &resource, ResourceProvider provider);
  std::vector<Resource> ListResources() const;
  async_simple::coro::Lazy<ResourceContent> ReadResource(
      std::string const &uri);
  bool HasResource(std::string const &uri) const;

  void SetSseCallback(SseEventCallback callback);

  private:
  ServerInfo server_info_;
  std::unordered_map<std::string, Tool> tools_;
  std::unordered_map<std::string, ToolHandler> tool_handlers_;
  mutable std::shared_mutex tools_mutex_;

  std::unordered_map<std::string, Resource> resources_;
  std::unordered_map<std::string, ResourceProvider> resource_providers_;
  mutable std::shared_mutex resource_mutex_;

  SseEventCallback sse_callback_;
  mutable std::mutex sse_mutex_;
};
}  // namespace mcp
