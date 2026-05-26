#include "http_jsonrpc.h"
#include <optional>
#include <utility>
#include "async_simple/coro/Lazy.h"
#include "cinatra.hpp"
#include "cinatra/coro_http_request.hpp"
#include "cinatra/coro_http_response.hpp"
#include "cinatra/response_cv.hpp"
#include "config.h"
#include "json_helper.hpp"
#include "logger.h"

namespace mcp {

static json jsonrpc_error_object(int code, std::string_view message,
                                 json id = nullptr) {
  return json{
      {"jsonrpc", "2.0"},
      {"error",
       {
           {"code", code},
           {"message", std::string(message)},
       }},
      {"id", std::move(id)},
  };
}

static std::string jsonrpc_error(int code, std::string_view message,
                                 json id = nullptr) {
  return jsonrpc_error_object(code, message, std::move(id)).dump();
}

static JsonRpcRequest parse_jsonrpc_request(json const &request_json) {
  if (!request_json.is_object()) {
    throw std::runtime_error("Invalid request");
  }

  JsonRpcRequest request;
  request.jsonrpc = request_json.value("jsonrpc", "2.0");
  request.method = request_json.at("method").get<std::string>();
  if (request_json.contains("id")) {
    request.id = request_json["id"];
  }
  if (request_json.contains("params")) {
    request.params = request_json["params"];
  }
  return request;
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
  Init();
}

HttpJsonRpcServer::HttpJsonRpcServer(JsonRpcDispatcher dispatcher,
                                     std::string const &host, int port,
                                     int thread_num)
    : dispatcher_(dispatcher),
      host_(host),
      port_(port),
      thread_num_(thread_num) {
  impl_ = std::make_unique<Impl>(thread_num_, port_, host_);
  Init();
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
}

void HttpJsonRpcServer::Stop() {
  if (!running_) {
    return;
  }
  running_.store(false);
  impl_->server.stop();
}

void HttpJsonRpcServer::Init() {
  using namespace cinatra;
  impl_->server.set_http_handler<POST>(
      "/jsonrpc",
      [this](coro_http_request &req,
             coro_http_response &resp) -> async_simple::coro::Lazy<void> {
        resp.add_header("Access-Control-Allow-Origin", "*");
        resp.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp.add_header("Access-Control-Allow-Headers", "Content-Type");
        resp.add_header("Content-Type", "application/json");
        try {
          std::string response =
              co_await HandleRequest(std::string{req.get_body()});
          if (response.empty()) {
            resp.set_status(status_type::no_content);
          } else {
            resp.set_status_and_content(status_type::ok, response);
          }
        } catch (std::exception const &e) {
          MCP_LOG_ERROR("Error handing request:{}", e.what());
          resp.set_status_and_content(status_type::ok,
                                      jsonrpc_error(-32603, e.what()));
        }
        co_return;
      });
  impl_->server.set_http_handler<OPTIONS>(
      "/jsonrpc", [](coro_http_request &req, coro_http_response &resp) {
        resp.add_header("Access-Control-Allow-Origin", "*");
        resp.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp.add_header("Access-Control-Allow-Headers", "Content-Type");
        resp.set_status(status_type::no_content);
      });
  impl_->server.set_http_handler<GET>(
      "/health", [](coro_http_request &req, coro_http_response &resp) {
        nlohmann::json health = {{"status", "ok"},
                                 {"service", "mcp-http-jsonrpc"},
                                 {"timestamp", std::time(nullptr)}};
        resp.add_header("Access-Control-Allow-Headers", "Content-Type");
        resp.set_status_and_content(status_type::ok, health.dump());
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
        resp.add_header("Access-Control-Allow-Headers", "Content-Type");
        resp.set_status_and_content(status_type::ok, info.dump());
      });
}

void HttpJsonRpcServer::RegisterSseEndpoint(std::string const &path,
                                            SseCallback callback) {
  using namespace cinatra;
  impl_->server.set_http_handler<GET>(
      path,
      [callback](coro_http_request &req,
                 coro_http_response &resp) -> async_simple::coro::Lazy<void> {
        resp.add_header("Access-Control-Allow-Origin", "*");
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
  MCP_LOG_DEBUG("Request body: {}", request_body);

  auto handle_single_request = [this](JsonRpcRequest const &request)
      -> async_simple::coro::Lazy<std::optional<JsonRpcResponse>> {
    bool const has_id = request.id.has_value();
    json id = has_id ? request.id.value() : json(nullptr);

    if (!dispatcher_.HasHandler(request.method)) {
      if (!has_id) {
        co_return std::nullopt;
      }
      JsonRpcResponse response;
      response.id = std::move(id);
      response.error =
          JsonRpcError{.code = jsonrpc_errc::MethodNotFound,
                       .message = "Method not found: " + request.method};
      co_return std::optional<JsonRpcResponse>{std::move(response)};
    }

    try {
      auto params = request.params.value_or(json::object());
      auto result = co_await dispatcher_.Call(request.method, params);
      if (!has_id) {
        co_return std::nullopt;
      }

      JsonRpcResponse response;
      response.id = std::move(id);
      response.result = std::move(result);
      co_return std::optional<JsonRpcResponse>{std::move(response)};
    } catch (std::exception const &e) {
      if (!has_id) {
        co_return std::nullopt;
      }
      JsonRpcResponse response;
      response.id = std::move(id);
      response.error = JsonRpcError{.code = jsonrpc_errc::InternalError,
                                    .message = e.what()};
      co_return std::optional<JsonRpcResponse>{std::move(response)};
    }
  };

  try {
    auto request_json = json::parse(request_body);
    if (request_json.is_array()) {
      json batch_response = json::array();
      for (auto const &single_request_json: request_json) {
        try {
          auto request = parse_jsonrpc_request(single_request_json);
          auto response = co_await handle_single_request(request);
          if (response.has_value()) {
            batch_response.push_back(
                json_helper::reflect_to_json(response.value()));
          }
        } catch (std::exception const &e) {
          MCP_LOG_WARN("Error in batch request: {}", e.what());
          batch_response.push_back(jsonrpc_error_object(
              jsonrpc_errc::InvalidRequest, "Invalid request"));
        }
      }
      if (batch_response.empty()) {
        co_return std::string{};
      }
      co_return batch_response.dump();
    }

    auto request = parse_jsonrpc_request(request_json);
    auto response = co_await handle_single_request(request);
    if (!response.has_value()) {
      co_return std::string{};
    }
    co_return json_helper::reflect_to_json(response.value()).dump();
  } catch (json::parse_error const &) {
    co_return jsonrpc_error(jsonrpc_errc::ParseError, "Parse error");
  } catch (std::exception const &e) {
    MCP_LOG_WARN("Invalid request: {}", e.what());
    co_return jsonrpc_error(jsonrpc_errc::InvalidRequest, "Invalid request");
  }
}
}  // namespace mcp
