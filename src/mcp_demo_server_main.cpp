#include <atomic>
#include <chrono>
#include <csignal>
#include <ctime>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "async_simple/coro/Lazy.h"
#include "async_simple/coro/SyncAwait.h"
#include "cinatra/ylt/coro_io/coro_io.hpp"
#include "config.h"
#include "json_helper.hpp"
#include "json_rpc/http_jsonrpc.h"
#include "json_rpc/jsonrpc.h"
#include "logger.h"
#include "mcp/mcp_server.h"
#include "mcp/types.h"

namespace {

using namespace std::chrono_literals;

std::atomic_bool g_running{true};
std::unique_ptr<mcp::HttpJsonRpcServer> g_http_server;

struct CliOptions {
  std::string config_file = "resources/config.json";
  std::string mode = "both";
  std::string host;
  int port = 0;
  int thread_num = 0;
  bool help = false;
};

class EventQueue {
  public:
  void Push(mcp::json event) {
    std::lock_guard lock(mutex_);
    events_.push_back(std::move(event));
  }

  std::vector<mcp::json> Drain() {
    std::lock_guard lock(mutex_);
    std::vector<mcp::json> result;
    result.swap(events_);
    return result;
  }

  private:
  std::mutex mutex_;
  std::vector<mcp::json> events_;
};

void HandleSignal(int) {
  g_running.store(false);
}

std::string FormatLocalTime() {
  std::time_t now = std::time(nullptr);
  std::tm time_info{};
  localtime_r(&now, &time_info);

  char buffer[64]{};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &time_info);
  return buffer;
}

std::string LogLevelName(spdlog::level::level_enum level) {
  auto view = spdlog::level::to_string_view(level);
  return std::string(view.data(), view.size());
}

mcp::ToolResult ErrorResult(std::string message) {
  return mcp::ToolResult{.content{{.type = "text", .text = std::move(message)}},
                         .type = mcp::ToolResultType::error};
}

mcp::ToolResult TextResult(std::string text) {
  return mcp::ToolResult{.content{{.type = "text", .text = std::move(text)}},
                         .type = mcp::ToolResultType::result};
}

bool IsSafeRelativePath(std::filesystem::path const &path) {
  if (path.empty() || path.is_absolute()) {
    return false;
  }
  for (auto const &part: path) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

mcp::ToolResult WriteFileBlocking(std::filesystem::path relative_path,
                                  std::string content) {
  try {
    auto output_root = std::filesystem::absolute("mcp_demo_output");
    auto target = (output_root / relative_path).lexically_normal();
    std::filesystem::create_directories(target.parent_path());

    std::ofstream file(target, std::ios::binary);
    if (!file.is_open()) {
      return ErrorResult("Failed to open output file");
    }
    file << content;
    return TextResult("Successfully wrote to file: " + target.string());
  } catch (std::exception const &e) {
    return ErrorResult(std::format("Error writing file: {}", e.what()));
  }
}

CliOptions ParseOptions(int argc, char *argv[]) {
  CliOptions options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    auto require_value = [&](std::string const &name) -> std::string {
      if (i + 1 >= argc) {
        throw std::runtime_error(std::format("missing value for {}", name));
      }
      return argv[++i];
    };

    if (arg == "--config") {
      options.config_file = require_value(arg);
    } else if (arg == "--mode") {
      options.mode = require_value(arg);
    } else if (arg == "--host") {
      options.host = require_value(arg);
    } else if (arg == "--port") {
      options.port = std::stoi(require_value(arg));
    } else if (arg == "--threads") {
      options.thread_num = std::stoi(require_value(arg));
    } else if (arg == "--help" || arg == "-h") {
      options.help = true;
    } else {
      throw std::runtime_error(std::format("unknown option: {}", arg));
    }
  }
  return options;
}

void PrintUsage(char const *program) {
  std::cout << "Usage: " << program << " [OPTIONS]\n"
            << "Options:\n"
            << "  --mode MODE      Server mode: http, stdio, or both\n"
            << "  --config FILE    Configuration file path\n"
            << "  --host HOST      HTTP host override\n"
            << "  --port PORT      HTTP port override\n"
            << "  --threads NUM    HTTP worker thread count override\n"
            << "  --help, -h       Show this help\n";
}

void ConfigureLogging(bool config_loaded) {
  auto &config = mcp::Config::GetInstance();
  if (config_loaded) {
    mcp::Logger::GetInstance().Init(
        "mcp_server", config.GetLogFilePath(), config.GetLogFileSize(),
        config.GetLogFileCount(), config.GetLogConsoleOutput());
    mcp::Logger::GetInstance().SetLevel(config.GetLogLevel());
    return;
  }

  mcp::Logger::GetInstance().Init("mcp_server", "logs/server.log",
                                  10 * 1024 * 1024, 5, true);
  mcp::Logger::GetInstance().SetLevel(spdlog::level::info);
}

void RegisterDemoTools(mcp::McpServer &server) {
  mcp::Tool echo{
      .name = "echo",
      .description = "Echo back the input message",
      .inputSchema = {.type = "object",
                      .properties = {{"message",
                                      {{"type", "string"},
                                       {"description", "Message to echo"}}}},
                      .required = {"message"}}};
  server.RegisterTool(
      echo,
      [](mcp::json const &args) -> async_simple::coro::Lazy<mcp::ToolResult> {
        if (!args.contains("message") || !args["message"].is_string()) {
          co_return ErrorResult("Missing or invalid 'message' parameter");
        }
        co_return TextResult("Echo: " + args["message"].get<std::string>());
      });

  mcp::Tool calculate{
      .name = "calculate",
      .description = "Perform basic arithmetic operations",
      .inputSchema = {
          .type = "object",
          .properties = {{"operation",
                          {{"type", "string"},
                           {"enum", mcp::json::array({"add", "subtract",
                                                      "multiply", "divide"})}}},
                         {"a", {{"type", "number"}}},
                         {"b", {{"type", "number"}}}},
          .required = {"operation", "a", "b"}}};
  server.RegisterTool(
      calculate,
      [](mcp::json const &args) -> async_simple::coro::Lazy<mcp::ToolResult> {
        if (!args.contains("operation") || !args["operation"].is_string() ||
            !args.contains("a") || !args["a"].is_number() ||
            !args.contains("b") || !args["b"].is_number()) {
          co_return ErrorResult("Missing or invalid calculator parameters");
        }

        auto operation = args["operation"].get<std::string>();
        double a = args["a"].get<double>();
        double b = args["b"].get<double>();
        double value = 0.0;

        if (operation == "add") {
          value = a + b;
        } else if (operation == "subtract") {
          value = a - b;
        } else if (operation == "multiply") {
          value = a * b;
        } else if (operation == "divide") {
          if (b == 0.0) {
            co_return ErrorResult("Division by zero");
          }
          value = a / b;
        } else {
          co_return ErrorResult("Unsupported operation: " + operation);
        }

        co_return TextResult(std::format("{}", value));
      });

  mcp::Tool get_time{
      .name = "get_time",
      .description = "Get current system time",
      .inputSchema = {
          .type = "object", .properties = mcp::json::object(), .required = {}}};
  server.RegisterTool(
      get_time,
      [](mcp::json const &) -> async_simple::coro::Lazy<mcp::ToolResult> {
        co_return TextResult(FormatLocalTime());
      });

  mcp::Tool get_weather{
      .name = "get_weather",
      .description = "Get weather information for a city",
      .inputSchema = {
          .type = "object",
          .properties = {{"city",
                          {{"type", "string"}, {"description", "City name"}}}},
          .required = {"city"}}};
  server.RegisterTool(
      get_weather,
      [](mcp::json const &args) -> async_simple::coro::Lazy<mcp::ToolResult> {
        if (!args.contains("city") || !args["city"].is_string()) {
          co_return ErrorResult("Missing or invalid 'city' parameter");
        }

        auto city = args["city"].get<std::string>();
        std::ostringstream weather_info;
        weather_info << "Weather Report for " << city << '\n'
                     << "Time: " << FormatLocalTime() << '\n'
                     << "Temperature: 22°C\n"
                     << "Condition: Sunny\n"
                     << "Humidity: 45%\n"
                     << "Wind: 5 km/h NE\n"
                     << "Note: This is simulated data.";
        co_return TextResult(weather_info.str());
      });

  mcp::Tool write_file{
      .name = "write_file",
      .description = "Write content to a relative file under mcp_demo_output",
      .inputSchema = {
          .type = "object",
          .properties =
              {{"path",
                {{"type", "string"}, {"description", "Relative output path"}}},
               {"content",
                {{"type", "string"}, {"description", "File content"}}}},
          .required = {"path", "content"}}};
  server.RegisterTool(
      write_file,
      [](mcp::json const &args) -> async_simple::coro::Lazy<mcp::ToolResult> {
        if (!args.contains("path") || !args["path"].is_string() ||
            !args.contains("content") || !args["content"].is_string()) {
          co_return ErrorResult("Missing or invalid file parameters");
        }

        std::filesystem::path relative_path = args["path"].get<std::string>();
        if (!IsSafeRelativePath(relative_path)) {
          co_return ErrorResult(
              "Path must be relative and stay under mcp_demo_output");
        }

        auto content = args["content"].get<std::string>();
        auto result = co_await coro_io::post(
            [relative_path = std::move(relative_path),
             content = std::move(content)]() mutable {
              return WriteFileBlocking(std::move(relative_path),
                                       std::move(content));
            });
        co_return std::move(result).value();
      });
}

void RegisterDemoResources(mcp::McpServer &server) {
  mcp::Resource system_info{.uri = "system://info",
                            .name = "System Information",
                            .description = "Basic server information",
                            .mimeType = "text/plain"};
  server.RegisterResource(
      system_info,
      [](std::string const &uri)
          -> async_simple::coro::Lazy<mcp::ResourceContent> {
        std::ostringstream content;
        content << "MCP Demo Server\n"
                << "Time: " << FormatLocalTime() << '\n';
        co_return mcp::ResourceContent{
            .uri = uri, .mimeType = "text/plain", .text = content.str()};
      });

  mcp::Resource server_config{.uri = "config://server",
                              .name = "Server Configuration",
                              .description = "Runtime server configuration",
                              .mimeType = "application/json"};
  server.RegisterResource(
      server_config,
      [](std::string const &uri)
          -> async_simple::coro::Lazy<mcp::ResourceContent> {
        auto &config = mcp::Config::GetInstance();
        mcp::json content{{"host", config.GetServerHost()},
                          {"port", config.GetServerPort()},
                          {"thread_num", config.GetServerThreadNum()},
                          {"log_level", LogLevelName(config.GetLogLevel())},
                          {"log_console_output", config.GetLogConsoleOutput()}};
        co_return mcp::ResourceContent{.uri = uri,
                                       .mimeType = "application/json",
                                       .text = content.dump(2)};
      });
}

void RegisterDemoPrompts(mcp::McpServer &server) {
  mcp::Prompt code_review{
      .name = "code_review",
      .description = "Generate a code review prompt",
      .arguments = {
          {.name = "code", .description = "Code to review", .required = true},
          {.name = "language",
           .description = "Programming language",
           .required = true}}};
  server.RegisterPrompt(
      code_review,
      [](mcp::json const &args)
          -> async_simple::coro::Lazy<std::vector<mcp::PromptMessage>> {
        if (!args.contains("code") || !args["code"].is_string() ||
            !args.contains("language") || !args["language"].is_string()) {
          throw std::runtime_error("Missing or invalid prompt arguments");
        }

        auto text = "Please review this " +
                    args["language"].get<std::string>() + " code:\n\n" +
                    args["code"].get<std::string>();
        co_return std::vector<mcp::PromptMessage>{mcp::PromptMessage{
            .role = mcp::Role::User,
            .content = {{"type", "text"}, {"text", std::move(text)}}}};
      });
}

void RegisterDemoContent(mcp::McpServer &server) {
  RegisterDemoTools(server);
  RegisterDemoResources(server);
  RegisterDemoPrompts(server);
}

mcp::JsonRpcDispatcher CreateDispatcher(mcp::McpServer &server,
                                        EventQueue *events = nullptr) {
  mcp::JsonRpcDispatcher dispatcher;

  dispatcher.RegisterHandler(
      "initialize",
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        co_return json_helper::reflect_to_json(server.GetInitializeResult());
      });

  dispatcher.RegisterHandler(
      "notifications/initialized",
      [](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        co_return mcp::json::object();
      });

  dispatcher.RegisterHandler(
      "ping", [](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        co_return mcp::json::object();
      });

  dispatcher.RegisterHandler(
      "tools/list",
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        auto tools_json = mcp::json::array();
        for (auto const &tool: server.ListTool()) {
          tools_json.push_back(json_helper::reflect_to_json(tool));
        }
        co_return mcp::json{{"tools", tools_json}};
      });

  dispatcher.RegisterHandler(
      "tools/call",
      [&server,
       events](mcp::json const &params) -> async_simple::coro::Lazy<mcp::json> {
        auto tool_name = params.at("name").get<std::string>();
        auto arguments = params.value("arguments", mcp::json::object());
        auto result = co_await server.CallTool(tool_name, arguments);
        if (events != nullptr) {
          events->Push(
              mcp::json{{"type", "tool_call"},
                        {"tool", tool_name},
                        {"error", result.type == mcp::ToolResultType::error},
                        {"timestamp", std::time(nullptr)}});
        }
        co_return json_helper::reflect_to_json(result);
      });

  dispatcher.RegisterHandler(
      "resources/list",
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        auto resources_json = mcp::json::array();
        for (auto const &resource: server.ListResources()) {
          resources_json.push_back(json_helper::reflect_to_json(resource));
        }
        co_return mcp::json{{"resources", resources_json}};
      });

  dispatcher.RegisterHandler(
      "resources/read",
      [&server](
          mcp::json const &params) -> async_simple::coro::Lazy<mcp::json> {
        auto uri = params.at("uri").get<std::string>();
        auto content = co_await server.ReadResource(uri);
        co_return mcp::json{
            {"contents",
             mcp::json::array({json_helper::reflect_to_json(content)})}};
      });

  dispatcher.RegisterHandler(
      "prompts/list",
      [&server](mcp::json const &) -> async_simple::coro::Lazy<mcp::json> {
        auto prompts_json = mcp::json::array();
        for (auto const &prompt: server.ListPrompts()) {
          prompts_json.push_back(json_helper::reflect_to_json(prompt));
        }
        co_return mcp::json{{"prompts", prompts_json}};
      });

  dispatcher.RegisterHandler(
      "prompts/get",
      [&server](
          mcp::json const &params) -> async_simple::coro::Lazy<mcp::json> {
        auto name = params.at("name").get<std::string>();
        auto arguments = params.value("arguments", mcp::json::object());
        auto messages = co_await server.GetPrompt(name, arguments);
        auto messages_json = mcp::json::array();
        for (auto const &message: messages) {
          messages_json.push_back(json_helper::reflect_to_json(message));
        }
        co_return mcp::json{{"messages", messages_json}};
      });

  return dispatcher;
}

void RegisterSseEndpoints(mcp::HttpJsonRpcServer &server,
                          mcp::McpServer &mcp_server, EventQueue &events) {
  server.RegisterSseEndpoint(
      "/sse/events",
      [&mcp_server](auto const &send_event) -> async_simple::coro::Lazy<void> {
        auto started_at = std::time(nullptr);
        co_await send_event(mcp::json{{"type", "connected"},
                                      {"message", "Server events stream"}}
                                .dump());
        while (g_running.load()) {
          auto now = std::time(nullptr);
          co_await send_event(mcp::json{
              {"type", "server_status"},
              {"timestamp", now},
              {"tools_count", mcp_server.ListTool().size()},
              {"resources_count", mcp_server.ListResources().size()},
              {"prompts_count", mcp_server.ListPrompts().size()},
              {"uptime_seconds",
               now - started_at}}.dump());
          co_await coro_io::sleep_for(5s);
        }
      });

  server.RegisterSseEndpoint(
      "/sse/tool_calls",
      [&events](auto const &send_event) -> async_simple::coro::Lazy<void> {
        co_await send_event(
            mcp::json{{"type", "connected"}, {"message", "Tool call stream"}}
                .dump());
        while (g_running.load()) {
          for (auto const &event: events.Drain()) {
            co_await send_event(event.dump());
          }
          co_await coro_io::sleep_for(250ms);
        }
      });
}

void StartHttpServer(mcp::McpServer &server, std::string const &host, int port,
                     int thread_num, EventQueue &events) {
  auto dispatcher = CreateDispatcher(server, &events);
  g_http_server = std::make_unique<mcp::HttpJsonRpcServer>(
      std::move(dispatcher), host, port, thread_num);
  RegisterSseEndpoints(*g_http_server, server, events);
  server.SetSseCallback(
      [&events](mcp::json const &event) { events.Push(event); });
  g_http_server->Run();
  MCP_LOG_INFO("HTTP server started on {}:{}", host, port);
}

void StopHttpServer() {
  if (g_http_server) {
    g_http_server->Stop();
    g_http_server.reset();
  }
}

void RunStdioServer(mcp::McpServer &server) {
  auto dispatcher = CreateDispatcher(server);
  mcp::StdioJsonRpcServer stdio_server(std::move(dispatcher));
  async_simple::coro::syncAwait(stdio_server.Run());
}

void WaitUntilStopped() {
  while (g_running.load()) {
    std::this_thread::sleep_for(200ms);
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  try {
    auto options = ParseOptions(argc, argv);
    if (options.help) {
      PrintUsage(argv[0]);
      return 0;
    }
    if (options.mode != "http" && options.mode != "stdio" &&
        options.mode != "both") {
      throw std::runtime_error("mode must be http, stdio, or both");
    }

    auto &config = mcp::Config::GetInstance();
    bool const config_loaded = config.LoadFromFile(options.config_file);
    ConfigureLogging(config_loaded);

    std::string host = options.host;
    int port = options.port;
    int thread_num = options.thread_num;
    if (host.empty()) {
      host = config_loaded ? config.GetServerHost() : "127.0.0.1";
    }
    if (port == 0) {
      port = config_loaded ? config.GetServerPort() : 8080;
    }
    if (thread_num == 0) {
      thread_num = config_loaded ? config.GetServerThreadNum() : 1;
    }

    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    mcp::McpServer server("co_mcp_demo_server", "1.0.0");
    mcp::ServerCapabilities capabilities;
    capabilities.tools = mcp::ServerCapabilities::ToolsCapability{false};
    capabilities.resources =
        mcp::ServerCapabilities::ResourcesCapability{false, false};
    capabilities.prompts = mcp::ServerCapabilities::PromptsCapability{false};
    server.SetCapabilities(capabilities);
    RegisterDemoContent(server);

    if (options.mode == "stdio") {
      RunStdioServer(server);
    } else if (options.mode == "http") {
      EventQueue events;
      StartHttpServer(server, host, port, thread_num, events);
      WaitUntilStopped();
      StopHttpServer();
    } else {
      EventQueue events;
      StartHttpServer(server, host, port, thread_num, events);
      RunStdioServer(server);
      g_running.store(false);
      StopHttpServer();
    }

    MCP_LOG_INFO("Server shutdown complete");
    mcp::Logger::GetInstance().Shutdown();
    return 0;
  } catch (std::exception const &e) {
    std::cerr << "Fatal error: " << e.what() << '\n';
    MCP_LOG_ERROR("Fatal error: {}", e.what());
    mcp::Logger::GetInstance().Shutdown();
    return 1;
  }
}
