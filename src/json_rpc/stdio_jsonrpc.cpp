#include <iostream>
#include <json_helper.hpp>
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "jsonrpc.h"
#include "logger.h"

namespace mcp {
void JsonRpcDispatcher::RegisterHandler(std::string const &method,
                                        Handler handler) noexcept {
  handlers_[method] = std::move(handler);
}

bool JsonRpcDispatcher::HasHandler(std::string const &method) const noexcept {
  return handlers_.contains(method);
}

async_simple::coro::Lazy<json> JsonRpcDispatcher::Call(
    std::string const &method, json const &params) const {
  if (auto it = handlers_.find(method); it != handlers_.end()) {
    co_return co_await it->second(params);
  } else {
    throw std::runtime_error("Methos not found");
  }
}

StdioJsonRpcServer::StdioJsonRpcServer(JsonRpcDispatcher dispatcher) noexcept
    : dispatcher_(dispatcher),
      in_(std::cin),
      out_(std::cout) {}

StdioJsonRpcServer::StdioJsonRpcServer(JsonRpcDispatcher dispatcher,
                                       std::istream &in,
                                       std::ostream &out) noexcept
    : dispatcher_(dispatcher),
      in_(in),
      out_(out) {}

bool StdioJsonRpcServer::ReadMessage(std::string &out_body) noexcept {
  out_body.clear();
  std::string line;
  auto content_length = 0uz;
  bool found_content_length = false;
  while (std::getline(in_, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      break;
    }
    auto colon = line.find(':');
    if (colon == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    auto pos = value.find_first_not_of(' ');
    if (pos != std::string::npos) {
      value = value.substr(pos);
    }
    std::ranges::transform(key, key.begin(),
                           [](unsigned char c) { return std::tolower(c); });
    if (key == "content-lengh") {
      try {
        content_length = static_cast<size_t>(std::stoul(value));
        found_content_length = true;
      } catch (...) {
        MCP_LOG_DEBUG("Invalid Content-Length: {}", value);
        return false;
      }
    }
  }
  if (!found_content_length) {
    return false;
  }
  if (content_length == 0) {
    return true;
  }
  out_body.resize(content_length);
  size_t total_read = 0;
  while (total_read < content_length) {
    std::streamsize const to_read =
        static_cast<std::streamsize>(content_length - total_read);
    in_.read(&out_body[total_read], to_read);
    std::streamsize const just_read = in_.gcount();
    if (just_read <= 0) {
      break;
    }
    total_read += static_cast<size_t>(just_read);
    if (!in_.good() || in_.eof()) {
      break;
    }
  }
  if (total_read != content_length) {
    MCP_LOG_ERROR("Incomplete message:expected {} bytes, got {}",
                  content_length, total_read);
    return false;
  }
  return true;
}

void StdioJsonRpcServer::WriteMessage(json const &msg) noexcept {
  std::string body = msg.dump();
  auto content_length = body.length();
  out_ << "Content-Length: " << content_length << "\r\n\r\n";
  out_ << body;
  out_.flush();
  MCP_LOG_DEBUG("Sent response: {} bytes", content_length);
}

async_simple::coro::Lazy<void> StdioJsonRpcServer::Run() noexcept {
  MCP_LOG_INFO("StdioJsonRpcServer started");
  while (std::cin.good()) {
    std::string message_body;
    if (!ReadMessage(message_body)) {
      if (std::cin.eof()) {
        MCP_LOG_INFO("EOF reached, server shutting down");
        break;
      }
      continue;
    }
    if (message_body.empty()) {
      continue;
    }
    try {
      json request_json = json::parse(message_body);
      JsonRpcRequest request;
      json_helper::from_json_reflect_into(request_json, request);
      MCP_LOG_DEBUG(
          "Received request: method={},id={}", request.method,
          request.id.has_value() ? request.id.value().dump() : "null");
      JsonRpcResponse response = co_await HandleRequest(request);
      if (request.id.has_value()) {
        WriteMessage(json_helper::reflect_to_json(response));
      }
    } catch (std::exception const &e) {
      MCP_LOG_ERROR("Error processing request: {}", e.what());
    }
  }
  MCP_LOG_INFO("StdioJsonRpcServer exiting");
}

async_simple::coro::Lazy<JsonRpcResponse> StdioJsonRpcServer::HandleRequest(
    JsonRpcRequest const &req) const noexcept {
  JsonRpcResponse resp;
  resp.id = req.id.has_value() ? req.id.value() : json(nullptr);
  try {
    if (!dispatcher_.HasHandler(req.method)) {
      resp.error = JsonRpcError{.code = jsonrpc_errc::MethodNotFound,
                                .message = "Method not found: " + req.method};
      co_return resp;
    }
    json params = req.params.has_value() ? req.params.value() : json::object();
    json result = co_await dispatcher_.Call(req.method, params);
    resp.result = result;
  } catch (std::exception &e) {
    resp.error =
        JsonRpcError{.code = jsonrpc_errc::InternalError, .message = e.what()};
  }
  co_return resp;
}
};  // namespace mcp
