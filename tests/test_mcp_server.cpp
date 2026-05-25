#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include "async_simple/coro/SyncAwait.h"
#include "mcp/mcp_server.h"
#include "mcp/types.h"

namespace {

mcp::Tool MakeTool(std::string name, std::string description) {
  return mcp::Tool{.name = std::move(name),
                   .description = std::move(description),
                   .inputSchema = {.type = "object",
                                   .properties = nlohmann::json::object(),
                                   .required = {}}};
}

std::vector<std::string> ToolNames(std::vector<mcp::Tool> const &tools) {
  std::vector<std::string> names;
  names.reserve(tools.size());
  for (auto const &tool: tools) {
    names.push_back(tool.name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

}  // namespace

TEST(McpServerTest, ReturnsInitializeMetadata) {
  mcp::McpServer server("test-server", "0.2.0");

  auto result = server.GetInitializeResult();

  EXPECT_EQ(result.protocolVersion,
            std::string(mcp::LATEST_PROTOCOL_VERSION));
  EXPECT_EQ(result.serverInfo.name, "test-server");
  EXPECT_EQ(result.serverInfo.version, "0.2.0");
  EXPECT_TRUE(result.capabilities.contains("tools"));
}

TEST(McpServerTest, RegistersAndListsTools) {
  mcp::McpServer server;
  auto echo = MakeTool("echo", "Echo text");
  auto add = MakeTool("add", "Add numbers");

  server.RegisterTool(echo, [](mcp::json const &)
                                -> async_simple::coro::Lazy<mcp::ToolResult> {
    co_return mcp::ToolResult{.content = {}, .type = mcp::ToolResultType::result};
  });
  server.RegisterTool(add, [](mcp::json const &)
                               -> async_simple::coro::Lazy<mcp::ToolResult> {
    co_return mcp::ToolResult{.content = {}, .type = mcp::ToolResultType::result};
  });

  EXPECT_TRUE(server.HasTool("echo"));
  EXPECT_TRUE(server.HasTool("add"));
  EXPECT_FALSE(server.HasTool("missing"));
  EXPECT_EQ(ToolNames(server.ListTool()),
            (std::vector<std::string>{"add", "echo"}));
}

TEST(McpServerTest, CallsRegisteredTool) {
  mcp::McpServer server;
  server.RegisterTool(
      MakeTool("echo", "Echo text"),
      [](mcp::json const &arguments) -> async_simple::coro::Lazy<mcp::ToolResult> {
        co_return mcp::ToolResult{
            .content{{.type = "text",
                      .text = arguments.at("message").get<std::string>()}},
            .type = mcp::ToolResultType::result};
      });

  auto result = async_simple::coro::syncAwait(
      server.CallTool("echo", nlohmann::json{{"message", "hello"}}));

  ASSERT_EQ(result.type, mcp::ToolResultType::result);
  ASSERT_EQ(result.content.size(), 1);
  EXPECT_EQ(result.content[0].type, "text");
  ASSERT_TRUE(result.content[0].text.has_value());
  EXPECT_EQ(result.content[0].text.value(), "hello");
}

TEST(McpServerTest, MissingToolReturnsError) {
  mcp::McpServer server;

  auto result = async_simple::coro::syncAwait(
      server.CallTool("missing", nlohmann::json::object()));

  EXPECT_EQ(result.type, mcp::ToolResultType::error);
  EXPECT_FALSE(result.content.empty());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
