#include "config.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>

namespace mcp {
using namespace std::string_literals;

Config &Config::GetInstance() noexcept {
  static Config instance;
  return instance;
};

bool Config::LoadFromFile(std::string_view const path_string) noexcept {
  std::filesystem::path const config_file_path{path_string};
  if (!std::filesystem::exists(config_file_path) ||
      !std::filesystem::is_regular_file(config_file_path)) {
    std::println(stderr, "invalid path");
    return false;
  }
  try {
    std::ifstream config_file{config_file_path.string()};
    if (!config_file.is_open()) {
      std::println("could not open config file:{}", config_file_path);
      return false;
    }
    nlohmann::json config_data_;
    config_file >> config_data_;
    config_file.close();
    json_helper::from_json_reflect_into(config_data_, cfg_);
    load_ = true;
    return true;
  } catch (json::parse_error const &e) {
    std::println(stderr, "config json parse error:{}", e.what());
    return false;
  } catch (std::runtime_error const &e) {
    std::println(stderr, "config_json parse error{}", e.what());
    return false;
  }
  return false;
}
}  // namespace mcp
