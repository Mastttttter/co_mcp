#pragma once
#include <async_simple/coro/Lazy.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace mcp {
using namespace nlohmann;

struct JsonRpcError {
  int code;
  std::string message;
  std::optional<json> data;
};

struct JsonRpcRequest {
  std::string jsonrpc = "2.0";
  std::optional<json> id;
  std::string method;
  std::optional<json> params;
};

struct JsonRpcResponse {
  std::string jsonrpc = "2.0";
  nlohmann::json id;
  std::optional<json> result;
  std::optional<JsonRpcError> error;
};

namespace jsonrpc_errc {
constexpr int ParseError = -32700;
constexpr int InvalidRequest = -32600;
constexpr int MethodNotFound = -32601;
constexpr int InvalidParams = -32602;
constexpr int InternalError = -32603;
}  // namespace jsonrpc_errc

class JsonRpcDispatcher {
  public:
  using Handler = std::function<async_simple::coro::Lazy<json>(json const &)>;
  void RegisterHandler(std::string const &method, Handler handler) noexcept;
  bool HasHandler(std::string const &) const noexcept;
  async_simple::coro::Lazy<json> Call(std::string const &method,
                                      json const &params) const;

  private:
  std::unordered_map<std::string, Handler> handlers_;
};

class StdioJsonRpcServer {
  public:
  explicit StdioJsonRpcServer(JsonRpcDispatcher dispatcher) noexcept;
  StdioJsonRpcServer(JsonRpcDispatcher dispatcher, std::istream &in,
                     std::ostream &out) noexcept;
  async_simple::coro::Lazy<void> Run() noexcept;

  private:
  JsonRpcDispatcher dispatcher_;
  std::istream &in_;
  std::ostream &out_;
  bool ReadMessage(std::string &out_body) noexcept;
  void WriteMessage(json const &msg) noexcept;
  async_simple::coro::Lazy<JsonRpcResponse> HandleRequest(
      JsonRpcRequest const &req) const noexcept;
};
}  // namespace mcp
