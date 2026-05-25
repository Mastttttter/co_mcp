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

struct InitializeResult {
  std::string protocolVersion = std::string(LATEST_PROTOCOL_VERSION);
  json capabilities = json{{"tools", json::object()}};
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

}  // namespace mcp
