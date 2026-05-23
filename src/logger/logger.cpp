#include "logger.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include <vector>

namespace mcp {
Logger &Logger::GetInstance() noexcept {
  static Logger logger;
  return logger;
}

void Logger::Init(std::string_view logger_name, std::string_view log_file_path,
                  size_t max_file_size, size_t max_files, bool console_output) {
  if (initialized_) {
    return;
  }
  std::vector<spdlog::sink_ptr> sinks;
  if (console_output) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%H:%M:%S.%e] [%^%l%$] [%n] %v");
    sinks.emplace_back(std::move(console_sink));
  }
  if (!log_file_path.empty()) {
    std::filesystem::path file_path{log_file_path};
    auto dir = file_path.parent_path();
    std::filesystem::create_directories(dir);
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        std::string{log_file_path}, max_file_size, max_files);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [%t] %v");
    sinks.emplace_back(std::move(file_sink));
  }
  logger_ = std::make_shared<spdlog::logger>(std::string{logger_name},
                                             sinks.begin(), sinks.end());
  SetLevel(spdlog::level::info);
  logger_->flush_on(spdlog::level::info);
  spdlog::register_logger(logger_);
  initialized_ = true;
};

void Logger::SetLevel(spdlog::level::level_enum level) noexcept {
  logger_->set_level(level);
};

void Logger::Flush() {
  if (!initialized_ || !logger_) {
    return;
  }
  logger_->flush();
}

void Logger::Shutdown() {
  if (!initialized_ || !logger_) {
    return;
  }
  Flush();
  spdlog::drop(logger_->name());
  logger_.reset();
  initialized_ = false;
}
}  // namespace mcp
