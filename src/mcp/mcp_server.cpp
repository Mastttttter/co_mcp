#include "mcp_server.h"
#include <format>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <utility>
#include "logger.h"
#include "types.h"

namespace mcp {

McpServer::McpServer() : McpServer("co_mcp", "1.0.0") {}

McpServer::McpServer(std::string name, std::string version)
    : server_info_{.name = std::move(name), .version = std::move(version)} {}

InitializeResult McpServer::GetInitializeResult() const {
  return InitializeResult{
      .protocolVersion = std::string(LATEST_PROTOCOL_VERSION),
      .serverInfo = server_info_};
}

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
  std::shared_lock lock(tools_mutex_);
  return tools_ | std::views::transform([](auto const &in) -> auto const & {
           return in.second;
         }) |
         std::ranges::to<std::vector<Tool>>();
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
  std::shared_lock lock(tools_mutex_);
  return tools_.contains(name);
}

void McpServer::RegisterResource(Resource const &resource,
                                 ResourceProvider provider) {
  std::lock_guard lock(resource_mutex_);
  if (resources_.find(resource.uri) != resources_.end()) {
    MCP_LOG_WARN("Resource '{}' already registered", resource.uri);
  }
  resources_[resource.uri] = resource;
  resource_providers_[resource.uri] = std::move(provider);
  MCP_LOG_INFO("Resource registered: {}", resource.uri);
}

std::vector<Resource> McpServer::ListResources() const {
  std::shared_lock lock(resource_mutex_);
  return resources_ | std::views::transform([](auto const &in) -> auto const & {
           return in.second;
         }) |
         std::ranges::to<std::vector<Resource>>();
}

async_simple::coro::Lazy<ResourceContent> McpServer::ReadResource(
    std::string const &uri) {
  std::shared_lock lock(resource_mutex_);
  auto provider_it = resource_providers_.find(uri);
  if (provider_it == resource_providers_.end()) {
    throw std::runtime_error(std::format("Resource not found {}", uri));
  }
  try {
    co_return co_await provider_it->second(uri);
  } catch (std::exception const &e) {
    MCP_LOG_ERROR("Failed to read resource '{}' : {}", uri, e.what());
    throw;
  }
}

bool McpServer::HasResource(std::string const &uri) const {
  std::shared_lock lock(resource_mutex_);
  return resources_.contains(uri);
}

void McpServer::RegisterPrompt(Prompt const &prompt,
                               PromptGenerator generator) {
  std::lock_guard lock(prompt_mutex_);
  prompts_[prompt.name] = prompt;
  prompt_generators_[prompt.name] = std::move(generator);
  MCP_LOG_INFO("Prompt registered: {}", prompt.name);
}

std::vector<Prompt> McpServer::ListPrompts() const {
  std::shared_lock lock(prompt_mutex_);
  return prompts_ | std::views::transform([](auto const &in) -> auto const & {
           return in.second;
         }) |
         std::ranges::to<std::vector<Prompt>>();
}

async_simple::coro::Lazy<std::vector<PromptMessage>> McpServer::GetPrompt(
    std::string const &name, json const &arguments) {
  std::shared_lock lock(prompt_mutex_);
  auto gen_it = prompt_generators_.find(name);
  if (gen_it == prompt_generators_.end()) {
    throw std::runtime_error(std::format("Prompt not found: {}", name));
  }
  try {
    co_return co_await gen_it->second(arguments);
  } catch (std::exception const &e) {
    auto s = std::format("failed to generate prompt '{}':{}", name, e.what());
    MCP_LOG_ERROR("{}", s);
    throw std::runtime_error(s);
  }
}

bool McpServer::HasPrompt(std::string const &name) const {
  std::shared_lock lock(prompt_mutex_);
  return prompts_.contains(name);
}

void McpServer::SetSseCallback(SseEventCallback callback) {
  sse_callback_ = callback;
}
}  // namespace mcp
