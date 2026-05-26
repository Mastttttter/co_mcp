#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mcp {
using namespace nlohmann;

inline constexpr std::string_view LATEST_PROTOCOL_VERSION = "2025-06-18";

struct ServerInfo {
  std::string name;
  std::string version;
};

struct ServerCapabilities {
  struct ToolsCapability {
    bool list_changed = false;
  };

  struct ResourcesCapability {
    bool subscribe = false;
    bool list_changed = false;
  };

  struct PromptsCapability {
    bool list_changed = false;
  };

  std::optional<ToolsCapability> tools;
  std::optional<ResourcesCapability> resources;
  std::optional<PromptsCapability> prompts;
  std::optional<json> logging;
};

struct InitializeResult {
  std::string protocolVersion = std::string(LATEST_PROTOCOL_VERSION);
  ServerCapabilities capabilities;
  ServerInfo serverInfo;
};

struct ToolInputSchema {
  std::string type = "object";
  json properties;
  std::vector<std::string> required;
};

struct Tool {
  std::string name;
  std::string description;
  ToolInputSchema inputSchema;
};

struct ContentItem {
  std::string type;
  std::optional<std::string> text;
  std::optional<std::string> data;
  std::optional<std::string> mimeType;
  std::optional<std::string> uri;
};

enum class ToolResultType {
  error = 0,
  result = 1,
};

struct ToolResult {
  std::vector<ContentItem> content;
  ToolResultType type;
};

struct Resource {
  std::string uri;
  std::string name;
  std::optional<std::string> description;
  std::optional<std::string> mimeType;
};

struct ResourceContent {
  std::string uri;
  std::optional<std::string> mimeType;
  std::string text;
  std::optional<std::string> blob;
};

struct PromptArgument {
  std::string name;
  std::optional<std::string> description;
  bool required = false;
};

struct Prompt {
  std::string name;
  std::optional<std::string> description;
  std::vector<PromptArgument> arguments;
};

enum class Role {
  User,
  Assistant,
};

struct PromptMessage {
  Role role;
  json content;
};

}  // namespace mcp
