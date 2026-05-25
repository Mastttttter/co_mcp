#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace mcp {
using namespace nlohmann;

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
