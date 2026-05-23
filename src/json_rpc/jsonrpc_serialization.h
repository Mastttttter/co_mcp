#pragma once

#include "jsonrpc.h"

namespace mcp {

void to_json(nlohmann::json &j, JsonRpcRequest const &r);
void from_json(nlohmann::json const &j, JsonRpcRequest &r);
void to_json(nlohmann::json &j, JsonRpcError const &e);
void from_json(nlohmann::json const &j, JsonRpcError &e);
void to_json(nlohmann::json &j, JsonRpcResponse const &r);
void from_json(nlohmann::json const &j, JsonRpcResponse &r);

}  // namespace mcp
