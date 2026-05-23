#include "jsonrpc.h"

namespace mcp {
void JsonRpcDispatcher::RegisterHandler(std::string const &method,
                                        Handler handler) noexcept {
  handlers_[method] = std::move(handler);
}

bool JsonRpcDispatcher::HasHandler(std::string const &method) const noexcept {
  return handlers_.contains(method);
}

json JsonRpcDispatcher::Call(std::string const &method,
                             json const &params) const {
  if (auto it = handlers_.find(method); it != handlers_.end()) {
    return it->second(params);
  } else {
    throw std::runtime_error("Methos not found");
  }
}

};  // namespace mcp
