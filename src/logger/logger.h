#pragma once
#include <spdlog/spdlog.h>
#include <cstddef>
#include <memory>
#include <string_view>

namespace mcp {

class Logger {
  public:
  static Logger &GetInstance() noexcept;
  void Init(std::string_view logger_name = "mcp",
            std::string_view log_file_path = "",
            size_t max_file_size = 10 * 1024 * 1024, size_t max_files = 5,
            bool console_output = true);
  void SetLevel(spdlog::level::level_enum level) noexcept;

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
