#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include "logger.h"

namespace {
class LoggerTest : public ::testing::Test {
  protected:
  void SetUp() override {
    std::filesystem::remove_all(TestRoot());
  }

  void TearDown() override {
    mcp::Logger::GetInstance().Shutdown();
    std::filesystem::remove_all(TestRoot());
  }

  std::filesystem::path TestRoot() const {
    auto const *test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return std::filesystem::temp_directory_path() / "mcp_logger_tests" /
           test_info->name();
  }
};
}  // namespace

TEST_F(LoggerTest, InitRegistersNamedLogger) {
  auto &logger = mcp::Logger::GetInstance();
  logger.Init("mcp_test_register", "", 1024, 1, false);

  auto registered = spdlog::get("mcp_test_register");
  ASSERT_NE(registered, nullptr);
  EXPECT_EQ(registered->name(), "mcp_test_register");
  EXPECT_EQ(registered->level(), spdlog::level::info);
}

TEST_F(LoggerTest, SetLevelUpdatesRegisteredLogger) {
  auto &logger = mcp::Logger::GetInstance();
  logger.Init("mcp_test_level", "", 1024, 1, false);
  logger.SetLevel(spdlog::level::debug);

  auto registered = spdlog::get("mcp_test_level");
  ASSERT_NE(registered, nullptr);
  EXPECT_EQ(registered->level(), spdlog::level::debug);
}

TEST_F(LoggerTest, InitCreatesFileLogger) {
  auto &logger = mcp::Logger::GetInstance();
  auto log_file = TestRoot() / "logs" / "server.log";

  logger.Init("mcp_test_file", log_file.string(), 1024 * 1024, 1, false);
  auto registered = spdlog::get("mcp_test_file");
  ASSERT_NE(registered, nullptr);

  registered->info("hello from logger test");
  logger.Flush();

  EXPECT_TRUE(std::filesystem::exists(log_file));
}

TEST_F(LoggerTest, ShutdownDropsRegisteredLogger) {
  auto &logger = mcp::Logger::GetInstance();
  logger.Init("mcp_test_shutdown", "", 1024, 1, false);
  ASSERT_NE(spdlog::get("mcp_test_shutdown"), nullptr);

  logger.Shutdown();

  EXPECT_EQ(spdlog::get("mcp_test_shutdown"), nullptr);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
