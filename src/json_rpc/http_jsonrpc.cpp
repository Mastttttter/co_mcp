#include "http_jsonrpc.h"
#include "async_simple/coro/Lazy.h"
#include "cinatra.hpp"
#include "cinatra/coro_http_request.hpp"
#include "cinatra/coro_http_response.hpp"
#include "cinatra/response_cv.hpp"
#include "config.h"
#include "logger.h"

namespace mcp {

static std::string jsonrpc_error(int code, std::string_view message,
                                 json id = nullptr) {
  return json{
      {"jsonrpc", "2.0"},
      {"error",
       {
           {"code", code},
           {"message", std::string(message)},
       }},
      {"id", std::move(id)},
  }
      .dump();
}

class HttpJsonRpcServer::Impl {
  public:
  cinatra::coro_http_server server;

  explicit Impl(size_t thread_num, unsigned short port, std::string const &host)
      : server(thread_num, port, host) {
    using namespace cinatra;
    server.set_default_handler(
        [](coro_http_request &req,
           coro_http_response &resp) -> async_simple::coro::Lazy<void> {
          nlohmann::json id = nullptr;
          try {
            auto body = req.get_body();
            if (!body.empty()) {
              auto j = json::parse(body);
              if (j.contains("id")) {
                id = j["id"];
              }
            }
          } catch (...) {}
          resp.add_header("Content-Type", "application/json");
          resp.set_status_and_content(
              status_type::not_found,
              jsonrpc_error(-32601, "Method not found", std::move(id)));
          co_return;
        });
    server.set_error_handler([](coro_http_request &, coro_http_response &resp,
                                std::string_view reason) {
      resp.add_header("Content-Type", "application/json");
      resp.set_status_and_content(
          status_type::service_unavailable,
          jsonrpc_error(-32603, "Internal server error"));
    });
  }
};

HttpJsonRpcServer::HttpJsonRpcServer(JsonRpcDispatcher dispatcher)
    : dispatcher_(dispatcher) {
  host_ = Config::GetInstance().GetServerHost();
  port_ = Config::GetInstance().GetServerPort();
  thread_num_ = Config::GetInstance().GetServerThreadNum();
  impl_ = std::make_unique<Impl>(thread_num_, port_, host_);
}

HttpJsonRpcServer::HttpJsonRpcServer(JsonRpcDispatcher dispatcher,
                                     std::string const &host, int port,
                                     int thread_num)
    : dispatcher_(dispatcher),
      host_(host),
      thread_num_(thread_num) {
  impl_ = std::make_unique<Impl>(thread_num_, port_, host_);
}

HttpJsonRpcServer::~HttpJsonRpcServer() {
  Stop();
};

void HttpJsonRpcServer::Run() {
  if (running_) {
    return;
  }
  running_.store(true);
  impl_->server.async_start();
  Init();
}

void HttpJsonRpcServer::Stop() {
  if (!running_) {
    return;
  }
  running_.store(false);
  impl_->server.stop();
  Init();
}

void HttpJsonRpcServer::Init() {
  using namespace cinatra;
  impl_->server.set_http_handler<GET>(
      "/jsonrpc",
      [this](coro_http_request &req,
             coro_http_response &resp) -> async_simple::coro::Lazy<void> {
        resp.add_header("Access-Control-Allow_Origin", "*");
        resp.add_header("Access-Control-Methods", "POST, OPTIONS");
        resp.add_header("Access-Control-Headers", "Content-Type");
        resp.add_header("Content-Type", "application/json");
        try {
          std::string response =
              co_await HandleRequest(std::string{req.get_body()});
          resp.set_status_and_content(status_type::ok, response);
        } catch (std::exception const &e) {
          MCP_LOG_ERROR("Error handing request:{}", e.what());
          resp.set_status_and_content(status_type::ok,
                                      jsonrpc_error(-32603, e.what()));
        }
        co_return;
      });
  impl_->server.set_http_handler<OPTIONS>(
      "/jsonrpc", [](coro_http_request &req, coro_http_response &resp) {
        resp.add_header("Access-Control-Allow_Origin", "*");
        resp.add_header("Access-Control-Methods", "POST, OPTIONS");
        resp.add_header("Access-Control-Headers", "Content-Type");
        resp.set_status(status_type::no_content);
      });
  impl_->server.set_http_handler<GET>(
      "/health", [](coro_http_request &req, coro_http_response &resp) {
        nlohmann::json health = {{"status", "ok"},
                                 {"service", "mcp-http-jsonrpc"},
                                 {"timestamp", std::time(nullptr)}};
        resp.add_header("Access-Control-Headers", "Content-Type");
        resp.set_status_and_content(status_type::no_content, health.dump());
      });
  impl_->server.set_http_handler<GET>(
      "/", [](coro_http_request &req, coro_http_response &resp) {
        nlohmann::json info = {
            {"service", "MCP HTTP JSON-RPC Server"},
            {"version", "1.0.0"},
            {"endpoints",
             {
                 {{"path", "/jsonrpc"}, {"method", "POST"}},
                 {{"path", "/health"}, {"method", "GET"}},
                 {{"path", "/sse/events"}, {"method", "GET"}},
                 {{"path", "/sse/tool_calls"}, {"method", "GET"}},
                 {{"path", "/"}, {"method", "GET"}},
             }}};
        resp.add_header("Access-Control-Headers", "Content-Type");
        resp.set_status_and_content(status_type::no_content, info.dump());
      });
}

void HttpJsonRpcServer::RegisterSseEndpoint(std::string const &path,
                                            SseCallback callback) {
  using namespace cinatra;
  impl_->server.set_http_handler<GET>(
      path,
      [callback](coro_http_request &req,
                 coro_http_response &resp) -> async_simple::coro::Lazy<void> {
        resp.add_header("Access-Control-Allow_Origin", "*");
        resp.add_header("X-Accel-Buffering", "no");
        auto *conn = resp.get_conn();
        bool ok = co_await conn->begin_sse();
        if (!ok) {
          co_return;
        }
        int total_id = 1;
        auto send_event =
            [conn, &ok, &total_id](
                std::string const &data) -> async_simple::coro::Lazy<void> {
          sse_event event{
              .event = "data", .data = data, .id = std::to_string(total_id++)};
          ok = co_await conn->write_sse_event(event);
          if (!ok) {
            co_return;
          }
        };
        try {
          co_await callback(send_event);
        } catch (std::exception const &e) {
          MCP_LOG_ERROR("SSE callback error in {} turn :{}", total_id,
                        e.what());
        }
        co_await conn->end_sse();
      });
}

async_simple::coro::Lazy<std::string> HttpJsonRpcServer::HandleRequest(
    std::string const &request_body) {
  std::string s = "adf";
  co_return s;
}
}  // namespace mcp
