# co_mcp

`co_mcp` is a C++ MCP demo server and small MCP support library built around coroutine-friendly JSON-RPC request handling.

It provides:

- an MCP server registry for tools, resources, and prompts;
- JSON-RPC dispatch over HTTP and stdio;
- Server-Sent Events for HTTP status and tool-call events;
- demo tools, resources, and prompts for smoke testing;
- tests covering registry behavior and coroutine-safe callback execution.

## Requirements

- CMake 4.3.2 or newer
- GCC toolchain with C++26 reflection support
- `-freflection` support for `mcp_lib`
- Threads
- CURL
- nlohmann_json
- spdlog
- GTest

The repository vendors cinatra under `lib/cinatra` and uses async_simple through that dependency.

## Build

Configure from the repository root:

```bash
cmake -S . -B build
```

Build all targets:

```bash
cmake --build build
```

The main executable is:

```bash
./build/src/mcp_server
```

## Test

Run the CTest suite:

```bash
ctest --test-dir build --output-on-failure
```

The test target copies `resources/` beside the test binary after build.

## Run the demo server

Start HTTP mode on localhost:

```bash
./build/src/mcp_server --mode http --host 127.0.0.1 --port 8080 --threads 1
```

Run stdio mode:

```bash
./build/src/mcp_server --mode stdio
```

Run both transports:

```bash
./build/src/mcp_server --mode both --host 127.0.0.1 --port 8080 --threads 1
```

Available options:

```text
--mode MODE      Server mode: http, stdio, or both
--config FILE    Configuration file path
--host HOST      HTTP host override
--port PORT      HTTP port override
--threads NUM    HTTP worker thread count override
--help, -h       Show help
```

Default config path:

```text
resources/config.json
```

If no config is loaded, HTTP defaults to `127.0.0.1:8080` with one worker thread.

## HTTP endpoints

When running in HTTP mode, the demo server exposes:

| Endpoint | Method | Purpose |
|---|---:|---|
| `/jsonrpc` | POST | MCP JSON-RPC requests |
| `/health` | GET | Health check |
| `/` | GET | Endpoint discovery |
| `/sse/events` | GET | Periodic server status events |
| `/sse/tool_calls` | GET | Tool-call event stream |

## Demo MCP content

### Tools

| Tool | Description |
|---|---|
| `echo` | Echoes a string message |
| `calculate` | Runs basic arithmetic: add, subtract, multiply, divide |
| `get_time` | Returns the current local server time |
| `get_weather` | Returns simulated weather text for a city |
| `write_file` | Writes a safe relative path under `mcp_demo_output` |

`write_file` rejects absolute paths and paths containing `..`.

### Resources

| URI | Description |
|---|---|
| `system://info` | Demo server information |
| `config://server` | Runtime server configuration |

### Prompts

| Prompt | Description |
|---|---|
| `code_review` | Creates a code-review prompt from `language` and `code` arguments |

## Use with Claude Code as an MCP server

Start the server first:

```bash
./build/src/mcp_server --mode http --host 127.0.0.1 --port 8080 --threads 1
```

Register it in Claude Code:

```bash
claude mcp add --transport http --scope local co-mcp http://127.0.0.1:8080/jsonrpc
```

Check connection health:

```bash
claude mcp list
```

Expected output includes:

```text
co-mcp: http://127.0.0.1:8080/jsonrpc (HTTP) - ✓ Connected
```

After registration, Claude Code can call tools such as:

- `mcp__co-mcp__echo`
- `mcp__co-mcp__calculate`
- `mcp__co-mcp__get_time`
- `mcp__co-mcp__get_weather`
- `mcp__co-mcp__write_file`

Remove the local registration when done:

```bash
claude mcp remove --scope local co-mcp
```

## Smoke-test the HTTP server

See [`SERVER_TESTING.md`](SERVER_TESTING.md) for curl-based checks covering:

- health and endpoint discovery;
- JSON-RPC initialize, list, and call requests;
- SSE streams under a one-worker HTTP server;
- safe and unsafe file writes;
- resources and prompts;
- missing-tool error handling.

## Project layout

```text
src/
  config/              Runtime config loading
  json_rpc/            JSON-RPC dispatcher, HTTP transport, stdio transport
  logger/              spdlog wrapper
  mcp/                 MCP server registry and protocol types
  utilities/           Reflection JSON helper
examples/              Small HTTP tool demo
resources/             Default runtime config
tests/                 GTest regression tests
lib/cinatra/           Vendored HTTP and coroutine I/O dependency
```

## Library notes

The core server API stores tools, resources, and prompts in `mcp::McpServer`.

Handlers use `async_simple::coro::Lazy`, so request handlers can be written in synchronous style while still allowing coroutine suspension. Internal registry locks are released before user handlers, providers, generators, or SSE callbacks are invoked.

HTTP SSE handlers use cinatra coroutine timers and avoid blocking HTTP worker threads while streams are open.
