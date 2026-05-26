#include <gtest/gtest.h>
#include <algorithm>
#include <optional>
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

async_simple::coro::Lazy<mcp::ToolResult> SuccessfulTool(mcp::json const &) {
  co_return mcp::ToolResult{.content = {}, .type = mcp::ToolResultType::result};
}

mcp::Resource MakeResource(std::string uri, std::string name) {
  return mcp::Resource{.uri = std::move(uri),
                       .name = std::move(name),
                       .description = std::nullopt,
                       .mimeType = "text/plain"};
}

mcp::Prompt MakePrompt(std::string name, std::string description) {
  return mcp::Prompt{.name = std::move(name),
                     .description = std::move(description),
                     .arguments = {}};
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

  EXPECT_EQ(result.protocolVersion, std::string(mcp::LATEST_PROTOCOL_VERSION));
  EXPECT_EQ(result.serverInfo.name, "test-server");
  EXPECT_EQ(result.serverInfo.version, "0.2.0");
}

TEST(McpServerTest, RegistersAndListsTools) {
  mcp::McpServer server;
  auto echo = MakeTool("echo", "Echo text");
  auto add = MakeTool("add", "Add numbers");

  server.RegisterTool(
      echo, [](mcp::json const &) -> async_simple::coro::Lazy<mcp::ToolResult> {
        co_return mcp::ToolResult{.content = {},
                                  .type = mcp::ToolResultType::result};
      });
  server.RegisterTool(
      add, [](mcp::json const &) -> async_simple::coro::Lazy<mcp::ToolResult> {
        co_return mcp::ToolResult{.content = {},
                                  .type = mcp::ToolResultType::result};
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
      [](mcp::json const &arguments)
          -> async_simple::coro::Lazy<mcp::ToolResult> {
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

TEST(McpServerTest, SseCallbackCanReadToolsDuringRegisterTool) {
  mcp::McpServer server;
  bool observed = false;
  server.SetSseCallback([&](mcp::json const &event) {
    if (event.at("type").get<std::string>() != "tools_changed") {
      return;
    }
    observed = true;
    EXPECT_TRUE(server.HasTool("echo"));
    EXPECT_EQ(ToolNames(server.ListTool()), (std::vector<std::string>{"echo"}));
  });

  server.RegisterTool(MakeTool("echo", "Echo text"), SuccessfulTool);

  EXPECT_TRUE(observed);
}

TEST(McpServerTest, ToolHandlerCanReenterToolRegistryDuringCallTool) {
  mcp::McpServer server;
  server.RegisterTool(
      MakeTool("outer", "Outer tool"),
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::ToolResult> {
        server.RegisterTool(MakeTool("inner", "Inner tool"), SuccessfulTool);
        co_return mcp::ToolResult{.content = {},
                                  .type = mcp::ToolResultType::result};
      });

  auto result = async_simple::coro::syncAwait(
      server.CallTool("outer", nlohmann::json::object()));

  EXPECT_EQ(result.type, mcp::ToolResultType::result);
  EXPECT_TRUE(server.HasTool("inner"));
}

TEST(McpServerTest, ResourceProviderCanReenterResourceMetadataDuringReadResource) {
  mcp::McpServer server;
  server.RegisterResource(
      MakeResource("resource://one", "One"),
      [&server](std::string const &uri)
          -> async_simple::coro::Lazy<mcp::ResourceContent> {
        EXPECT_TRUE(server.HasResource(uri));
        EXPECT_EQ(server.ListResources().size(), 1);
        co_return mcp::ResourceContent{.uri = uri,
                                       .mimeType = "text/plain",
                                       .text = "ok"};
      });

  auto content = async_simple::coro::syncAwait(
      server.ReadResource("resource://one"));

  EXPECT_EQ(content.text, "ok");
}

TEST(McpServerTest, PromptGeneratorCanReenterPromptMetadataDuringGetPrompt) {
  mcp::McpServer server;
  server.RegisterPrompt(
      MakePrompt("review", "Review prompt"),
      [&server](mcp::json const &)
          -> async_simple::coro::Lazy<std::vector<mcp::PromptMessage>> {
        EXPECT_TRUE(server.HasPrompt("review"));
        EXPECT_EQ(server.ListPrompts().size(), 1);
        co_return std::vector<mcp::PromptMessage>{mcp::PromptMessage{
            .role = mcp::Role::User,
            .content = mcp::json{{"type", "text"}, {"text", "ok"}}}};
      });

  auto messages = async_simple::coro::syncAwait(
      server.GetPrompt("review", nlohmann::json::object()));

  ASSERT_EQ(messages.size(), 1);
  EXPECT_EQ(messages[0].role, mcp::Role::User);
}

TEST(McpServerTest, CallToolSseEventsRunOutsideRegistryLocks) {
  mcp::McpServer server;
  server.RegisterTool(MakeTool("echo", "Echo text"), SuccessfulTool);

  bool registered_from_callback = false;
  std::vector<std::string> event_types;
  server.SetSseCallback([&](mcp::json const &event) {
    auto type = event.at("type").get<std::string>();
    event_types.push_back(type);
    EXPECT_TRUE(server.HasTool("echo"));
    if (type == "tool_call_start" && !registered_from_callback) {
      registered_from_callback = true;
      server.RegisterTool(MakeTool("callback", "Callback tool"),
                          SuccessfulTool);
    }
  });

  auto result = async_simple::coro::syncAwait(
      server.CallTool("echo", nlohmann::json::object()));

  EXPECT_EQ(result.type, mcp::ToolResultType::result);
  ASSERT_GE(event_types.size(), 3);
  EXPECT_EQ(event_types.front(), "tool_call_start");
  EXPECT_TRUE(std::find(event_types.begin(), event_types.end(),
                        "tool_call_end") != event_types.end());
  EXPECT_TRUE(server.HasTool("callback"));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
