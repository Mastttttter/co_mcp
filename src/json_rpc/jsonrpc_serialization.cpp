#include "jsonrpc_serialization.h"

namespace mcp {

void to_json(nlohmann::json &j, JsonRpcRequest const &r) {
  j = nlohmann::json{{"jsonrpc", r.jsonrpc}, {"method", r.method}};

  if (r.id.has_value()) {
    j["id"] = r.id.value();
  }
  if (r.params.has_value()) {
    j["params"] = r.params.value();
  }
}

void from_json(nlohmann::json const &j, JsonRpcRequest &r) {
  r.jsonrpc = j.value("jsonrpc", "2.0");
  r.method = j.at("method").get<std::string>();

  if (j.contains("id")) {
    r.id = j["id"];
  } else {
    r.id.reset();
  }
  if (j.contains("params")) {
    r.params = j["params"];
  } else {
    r.params.reset();
  }
}

void to_json(nlohmann::json &j, JsonRpcError const &e) {
  j = nlohmann::json{{"code", e.code}, {"message", e.message}};

  if (e.data.has_value()) {
    j["data"] = e.data.value();
  }
}

void from_json(nlohmann::json const &j, JsonRpcError &e) {
  e.code = j.at("code").get<int>();
  e.message = j.at("message").get<std::string>();

  if (j.contains("data")) {
    e.data = j["data"];
  } else {
    e.data.reset();
  }
}

void to_json(nlohmann::json &j, JsonRpcResponse const &r) {
  j = nlohmann::json{{"jsonrpc", r.jsonrpc}, {"id", r.id}};

  if (r.result.has_value()) {
    j["result"] = r.result.value();
  }
  if (r.error.has_value()) {
    j["error"] = r.error.value();
  }
}

void from_json(nlohmann::json const &j, JsonRpcResponse &r) {
  r.jsonrpc = j.value("jsonrpc", "2.0");
  r.id = j.at("id");

  if (j.contains("result")) {
    r.result = j["result"];
  } else {
    r.result.reset();
  }
  if (j.contains("error")) {
    r.error = j["error"].get<JsonRpcError>();
  } else {
    r.error.reset();
  }
}

}  // namespace mcp
