#include <thread>
#include "async_simple/coro/Lazy.h"
#include "config.h"
#include "json_helper.hpp"
#include "json_rpc/http_jsonrpc.h"
#include "json_rpc/jsonrpc.h"
#include "logger.h"
#include "mcp/mcp_server.h"
#include "mcp/types.h"

int main() {
  mcp::Config::GetInstance().LoadFromFile("resources/config.json");
  mcp::Logger::GetInstance().Init();

  mcp::JsonRpcDispatcher dispatcher;
  mcp::McpServer mcp;
  mcp::Tool tool{.name = "testTool"};
  mcp.RegisterTool(
      tool,
      [](nlohmann::json const &args)
          -> async_simple::coro::Lazy<mcp::ToolResult> {
        try {
          if (!args.contains("message") || !args["message"].is_string()) {
            mcp::ToolResult error{
                .content{{.type = "text",
                          .text = "Missing or invalid 'message' parameter"}},
                .type = mcp::ToolResultType::error};
            co_return error;
          }
          std::string msg = args["message"];
          co_return mcp::ToolResult{.content{{.type = "text", .text = msg}},
                                    .type = mcp::ToolResultType::result};
        } catch (std::exception &e) {
          co_return mcp::ToolResult{
              .content{{.type = "text", .text = e.what()}},
              .type = mcp::ToolResultType::error};
        }
      });
  dispatcher.RegisterHandler(
      "tools/list",
      [&mcp](nlohmann::json const &params)
          -> async_simple::coro::Lazy<nlohmann::json> {
        MCP_LOG_DEBUG("Handing tools/list");
        auto tools = mcp.ListTool();
        auto tools_json = nlohmann::json::array();
        for (auto const &tool: tools) {
          tools_json.push_back(json_helper::reflect_to_json(tool));
        }
        co_return nlohmann::json{{{"tools", tools_json}}};
      });
  dispatcher.RegisterHandler(
      "tools/call",
      [&mcp](nlohmann::json const &params)
          -> async_simple::coro::Lazy<nlohmann::json> {
        MCP_LOG_DEBUG("Handing tools/call: {}", params.dump(4));
        std::string tool_name = params.at("name").get<std::string>();
        nlohmann::json arguments =
            params.value("arguments", nlohmann::json::object());
        auto result = co_await mcp.CallTool(tool_name, arguments);
        co_return json_helper::reflect_to_json(result);
      });
  mcp::HttpJsonRpcServer server(dispatcher);
  server.RegisterSseEndpoint(
      "/sse/events",
      [](auto const &send_event) -> async_simple::coro::Lazy<void> {
        co_await send_event(mcp::json{{"event", "startup"}}.dump(4));
      });
  {
    using namespace mcp;
    Tool tool;
    tool.name = "echo";
    tool.description = "Echo back input";
    tool.inputSchema.properties = {{"message", {{"type", "string"}}}};
    tool.inputSchema.required = {"message"};
    mcp.RegisterTool(
        tool, [](json const &args) -> async_simple::coro::Lazy<ToolResult> {
          ToolResult result;
          result.content.push_back(ContentItem{
              .type = "text",
              .text = "Echo: " + args["message"].get<std::string>()});
          co_return result;
        });
  }
  server.Run();
  while (true);
  return 0;
}
