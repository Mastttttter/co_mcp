#pragma once
#include <atomic>
#include "async_simple/coro/Lazy.h"
#include "jsonrpc.h"

namespace mcp {
class HttpJsonRpcServer {
  public:
  explicit HttpJsonRpcServer(JsonRpcDispatcher dispatcher);
  HttpJsonRpcServer(JsonRpcDispatcher dispatcher, std::string const &host,
                    int port, int thread_num);
  virtual ~HttpJsonRpcServer();
  void Run();
  void Stop();
  using SseCallback = std::function<async_simple::coro::Lazy<void>(
      std::function<async_simple::coro::Lazy<void>(std::string const &)> const
          &)>;
  void RegisterSseEndpoint(std::string const &path, SseCallback callback);

  private:
  void Init();
  class Impl;
  std::unique_ptr<Impl> impl_;
  JsonRpcDispatcher dispatcher_;
  std::string host_;
  int port_;
  int thread_num_;
  std::atomic<bool> running_{false};
  async_simple::coro::Lazy<std::string> HandleRequest(
      std::string const &request_body);
};
}  // namespace mcp
