#pragma once
#include <spdlog/spdlog.h>
#include <cstddef>
#include <memory>
#include <string_view>

#define MCP_LOG_DEBUG(fmt, ...) \
  do { \
    auto logger = ::mcp::Logger::GetInstance().GetLogger(); \
    if (logger) \
      logger->debug("[{}:{}]" fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
  } while (0)

#define MCP_LOG_ERROR(fmt, ...) \
  do { \
    auto logger = ::mcp::Logger::GetInstance().GetLogger(); \
    if (logger) \
      logger->error("[{}:{}]" fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
  } while (0)
#define MCP_LOG_INFO(fmt, ...) \
  do { \
    auto logger = ::mcp::Logger::GetInstance().GetLogger(); \
    if (logger) \
      logger->info("[{}:{}]" fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
  } while (0)

namespace mcp {

class Logger {
  public:
  static Logger &GetInstance() noexcept;
  void Init(std::string_view logger_name = "mcp",
            std::string_view log_file_path = "logs/server.log",
            size_t max_file_size = 10 * 1024 * 1024, size_t max_files = 5,
            bool console_output = true);
  void SetLevel(spdlog::level::level_enum level) noexcept;

  std::shared_ptr<spdlog::logger> GetLogger() const noexcept {
    return logger_;
  }

  void Flush();
  void Shutdown();

  private:
  Logger() noexcept = default;
  ~Logger() = default;
  Logger(Logger const &) = delete;
  Logger &operator=(Logger const &) = delete;
  std::shared_ptr<spdlog::logger> logger_;
  bool initialized_ = false;
};
}  // namespace mcp
