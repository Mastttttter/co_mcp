#include <future>
#include <format>
#include <string>
#include <utility>
#include "async_simple/coro/Lazy.h"
#include "json_helper.hpp"
#include "json_rpc/http_jsonrpc.h"
#include "logger.h"
#include "mcp/mcp_server.h"
#include "mcp/types.h"

namespace {

mcp::ToolResult ErrorResult(std::string message) {
  return mcp::ToolResult{.content{{.type = "text", .text = std::move(message)}},
                         .type = mcp::ToolResultType::error};
}

mcp::ToolResult TextResult(std::string text) {
  return mcp::ToolResult{.content{{.type = "text", .text = std::move(text)}},
                         .type = mcp::ToolResultType::result};
}

void RegisterDemoTools(mcp::McpServer &server) {
  mcp::Tool echo{.name = "echo",
                 .description = "Echo text input",
                 .inputSchema = {.type = "object",
                                 .properties = {{"message", {{"type", "string"}}}},
                                 .required = {"message"}}};
  server.RegisterTool(
      echo, [](mcp::json const &args) -> async_simple::coro::Lazy<mcp::ToolResult> {
        if (!args.contains("message") || !args["message"].is_string()) {
          co_return ErrorResult("Missing or invalid 'message' parameter");
        }
        co_return TextResult(args["message"].get<std::string>());
      });

  mcp::Tool add{.name = "add",
                .description = "Add two numbers",
                .inputSchema = {.type = "object",
                                .properties = {{"a", {{"type", "number"}}},
                                               {"b", {{"type", "number"}}}},
                                .required = {"a", "b"}}};
  server.RegisterTool(
      add, [](mcp::json const &args) -> async_simple::coro::Lazy<mcp::ToolResult> {
        if (!args.contains("a") || !args.contains("b") || !args["a"].is_number() ||
            !args["b"].is_number()) {
          co_return ErrorResult("Missing or invalid numeric parameters");
        }
        auto sum = args["a"].get<double>() + args["b"].get<double>();
        co_return TextResult(std::format("{}", sum));
      });
}

void RegisterToolHandlers(mcp::JsonRpcDispatcher &dispatcher,
                          mcp::McpServer &server) {
  dispatcher.RegisterHandler(
      "initialize",
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        co_return json_helper::reflect_to_json(server.GetInitializeResult());
      });

  dispatcher.RegisterHandler(
      "notifications/initialized",
      [](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        co_return mcp::json::object();
      });

  dispatcher.RegisterHandler(
      "tools/list",
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        auto tools_json = mcp::json::array();
        for (auto const &tool: server.ListTool()) {
          tools_json.push_back(json_helper::reflect_to_json(tool));
        }
        co_return mcp::json{{"tools", tools_json}};
      });

  dispatcher.RegisterHandler(
      "tools/call",
      [&server](mcp::json const &params) -> async_simple::coro::Lazy<mcp::json> {
        auto tool_name = params.at("name").get<std::string>();
        auto arguments = params.value("arguments", mcp::json::object());
        auto result = co_await server.CallTool(tool_name, arguments);
        co_return json_helper::reflect_to_json(result);
      });
}

}  // namespace

int main() {
  mcp::Logger::GetInstance().Init("mcp_tools_demo", "", 1024 * 1024, 1, true);

  mcp::McpServer server("mcp-tools-demo", "1.0.0");
  RegisterDemoTools(server);

  mcp::JsonRpcDispatcher dispatcher;
  RegisterToolHandlers(dispatcher, server);

  mcp::HttpJsonRpcServer http_server(dispatcher, "127.0.0.1", 8080, 1);
  http_server.Run();

  std::promise<void> wait_forever;
  wait_forever.get_future().wait();
  return 0;
}
