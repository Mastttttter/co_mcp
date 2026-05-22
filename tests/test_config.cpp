#include <gtest/gtest.h>
#include "config.h"

TEST(ConfigTest, LoadFromFile) {
  auto &config = mcp::Config::GetInstance();
  EXPECT_EQ(config.LoadFromFile("resources/in"), true);
  EXPECT_EQ(config.GetServerPort(), 8080);
  EXPECT_EQ(config.GetLogFilePath(), "logs/server.log");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
