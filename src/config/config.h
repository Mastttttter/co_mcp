#pragma once
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include "json_helper.hpp"

namespace mcp {
using json = nlohmann::json;

class Config {
  public:
  static Config &GetInstance() noexcept;

  bool LoadFromFile(std::string_view const config_file_path) noexcept;

  int GetServerPort() const {
    return cfg_.server.port;
  }

  std::string GetLogFilePath() const {
    return cfg_.logging.log_file_path;
  }

  spdlog::level::level_enum GetLogLevel() const {
    return cfg_.logging.log_level;
  }

  size_t GetLogFileSize() const {
    return cfg_.logging.log_file_size;
  }

  int GetLogFileCount() const {
    return cfg_.logging.log_file_count;
  }

  bool GetLogConsoleOutput() const {
    return cfg_.logging.log_console_output;
  }

  private:
  struct[[= json_helper::json_meta::serializable]] Server_ {
    [[= json_helper::json_meta::default_value<int>{8080}]] int port;
  };

  struct[[= json_helper::json_meta::serializable]] Logging_ {
    [[= json_helper::json_meta::default_string(
        "logs/server.log")]] std::string log_file_path;
    [[= json_helper::json_meta::default_value{
        spdlog::level::info}]] spdlog::level::level_enum log_level;
    [[= json_helper::json_meta::default_value{10 * 1024 *
                                              1024uz}]] size_t log_file_size;
    [[= json_helper::json_meta::default_value{5}]] int log_file_count;
    [[= json_helper::json_meta::default_value{false}]] bool log_console_output;
  };

  struct[[= json_helper::json_meta::serializable]] Config_ {
    Server_ server;
    Logging_ logging;
  };

  Config() = default;
  ~Config() = default;
  Config &operator=(Config const &) = delete;
  Config(Config const &) = delete;
  Config_ cfg_;
  bool load_;
};
}  // namespace mcp
