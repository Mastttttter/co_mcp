#include <thread>
#include "async_simple/coro/Lazy.h"
#include "config.h"
#include "json_rpc/http_jsonrpc.h"
#include "json_rpc/jsonrpc.h"
#include "logger.h"

int main() {
  mcp::Config::GetInstance().LoadFromFile("resources/config.json");
  mcp::Logger::GetInstance().Init();

  mcp::JsonRpcDispatcher dispatcher;
  mcp::HttpJsonRpcServer server(dispatcher);
  server.RegisterSseEndpoint(
      "/sse/events",
      [](auto const &send_event) -> async_simple::coro::Lazy<void> {
        co_await send_event(mcp::json{{"event", "startup"}}.dump(4));
      });
  server.Run();
    while (true);
  return 0;
}
