# Testing the MCP HTTP Server

This guide assumes the server has already been rebuilt and restarted in HTTP mode.

If needed, start a one-worker smoke-test server:

```bash
./build/src/mcp_server --mode http --host 127.0.0.1 --port 8080 --threads 1
```

Set the base URL used by the commands below:

```bash
export BASE_URL=http://127.0.0.1:8080
```

## 1. Check the server is alive

```bash
curl -fsS "$BASE_URL/health" | jq .
```

Expected result:

- JSON response contains `"status": "ok"`.

## 2. Check advertised endpoints

```bash
curl -fsS "$BASE_URL/" | jq .
```

Expected result:

- Response lists `/jsonrpc`, `/health`, `/sse/events`, and `/sse/tool_calls`.

## 3. Initialize JSON-RPC

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Response contains `protocolVersion`, `capabilities`, and `serverInfo`.

## 4. List demo tools

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Response includes tools such as `echo`, `calculate`, `get_time`, `get_weather`, and `write_file`.

## 5. Call a demo tool

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"echo","arguments":{"message":"hello"}}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Response contains `Echo: hello`.

## 6. Confirm SSE streams do not block JSON-RPC

Open this in terminal A:

```bash
curl -N "$BASE_URL/sse/events"
```

Open this in terminal B:

```bash
curl -N "$BASE_URL/sse/tool_calls"
```

Then run this in terminal C while both SSE streams stay open:

```bash
time curl --max-time 5 -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"echo","arguments":{"message":"sse smoke"}}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Terminal C returns quickly with `Echo: sse smoke`.
- Terminal A keeps receiving `server_status` events.
- Terminal B receives `tool_call_start`, `tool_call_end`, and `tool_call` events.
- With `--threads 1`, this confirms the SSE streams are not pinning the only HTTP worker.

## 7. Test the offloaded file-write tool

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"write_file","arguments":{"path":"smoke/hello.txt","content":"hello from smoke"}}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Check the written file:

```bash
test "$(cat mcp_demo_output/smoke/hello.txt)" = 'hello from smoke' && echo ok
```

Expected result:

- Tool response reports a successful write.
- File content check prints `ok`.

## 8. Check unsafe file paths are rejected

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"write_file","arguments":{"path":"../bad.txt","content":"bad"}}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Response is an error result.
- No file is written outside `mcp_demo_output`.

## 9. Read demo resources

List resources:

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":7,"method":"resources/list","params":{}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Read system info:

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":8,"method":"resources/read","params":{"uri":"system://info"}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Resource list includes `system://info`.
- Resource read returns text content for the demo server.

## 10. Get a demo prompt

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":9,"method":"prompts/get","params":{"name":"code_review","arguments":{"language":"C++","code":"int main() { return 0; }"}}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Response contains a user prompt asking for a C++ code review.

## 11. Check missing-tool error handling

```bash
curl -fsS \
  -H 'Content-Type: application/json' \
  -d '{"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"missing","arguments":{}}}' \
  "$BASE_URL/jsonrpc" | jq .
```

Expected result:

- Response is an error result.
- Server stays running.

## Pass criteria

The server passes the smoke test when:

- `/health` returns `ok`.
- JSON-RPC requests return valid responses.
- Two open SSE streams do not block tool calls, especially with `--threads 1`.
- `/sse/tool_calls` emits tool start and end events after a tool call.
- `write_file` succeeds for safe relative paths and rejects unsafe paths.
- Resource and prompt endpoints return expected demo content.
- Server logs show no crashes or handler deadlocks.
