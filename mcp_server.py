import json
import sys
import os
import subprocess
import threading
import time

# To allow use as MCP Server, we will wrap the bolt_insights_tool process
# in an MCP JSON-RPC handler script

# This script acts as a basic JSON-RPC 2.0 MCP server since the 'mcp' Python package
# is not installed.
# See: https://modelcontextprotocol.io/

# We'll maintain the bolt process here
class BoltInsightsWrapper:
    def __init__(self):
        self.process = None
        self.output_thread = None

    def start(self, scenario):
        if self.process is not None:
            self.stop()
        
        bolt_exec = os.path.expanduser("~/bolt/_build/Debug/bolt/tool/insights/bolt_insights_tool")
        
        # Start process with interactive command
        self.process = subprocess.Popen(
            [bolt_exec, "--command=interactive", f"--scenario={scenario}"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1 # Line buffered
        )

        # Wait for the "ready" signal
        try:
            line = self.process.stdout.readline()
            if "ready" not in line:
                return f"Failed to start properly: {line}"
        except Exception as e:
            return f"Error starting: {e}"

        return "Successfully started task and monitoring."

    def stop(self):
        if self.process is not None:
            try:
                self.process.stdin.write("exit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=2)
            except:
                self.process.terminate()
            self.process = None
        return "Session stopped."

    def poll(self):
        if self.process is None or self.process.poll() is not None:
            return "No active session. Please start a task first."
        
        try:
            self.process.stdin.write("poll\n")
            self.process.stdin.flush()
            line = self.process.stdout.readline()
            if not line:
                return "Session terminated."
            
            try:
                data = json.loads(line)
                return data
            except json.JSONDecodeError:
                return {"error": "Failed to parse tool output", "raw": line.strip()}
        except Exception as e:
            return {"error": f"Poll failed: {e}"}

    def cancel(self):
        if self.process is None or self.process.poll() is not None:
            return "No active session."
            
        try:
            self.process.stdin.write("cancel\n")
            self.process.stdin.flush()
            line = self.process.stdout.readline()
            if line:
                return json.loads(line)
            return "Sent cancel request."
        except Exception as e:
            return {"error": f"Cancel failed: {e}"}

bolt_wrapper = BoltInsightsWrapper()

def process_request(req):
    method = req.get("method")
    params = req.get("params", {})
    req_id = req.get("id")

    if method == "initialize":
        # Standard MCP initialization response
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {
                    "tools": {}
                },
                "serverInfo": {
                    "name": "bolt-insights",
                    "version": "1.0.0"
                }
            }
        }
        
    elif method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "tools": [
                    {
                        "name": "start_task",
                        "description": "Start a Bolt task and begin monitoring its insights.",
                        "inputSchema": {
                            "type": "object",
                            "properties": {
                                "scenario": {"type": "string", "description": "Scenario to run (e.g. 'default')"}
                            }
                        }
                    },
                    {
                        "name": "poll_insights",
                        "description": "Poll the currently running task for active insights and its state.",
                        "inputSchema": {
                            "type": "object",
                            "properties": {}
                        }
                    },
                    {
                        "name": "cancel_task",
                        "description": "Force cancel the currently running task (e.g. if insights are critical).",
                        "inputSchema": {
                            "type": "object",
                            "properties": {}
                        }
                    }
                ]
            }
        }
        
    elif method == "tools/call":
        tool_name = params.get("name")
        tool_args = params.get("arguments", {})
        
        result_content = ""
        is_error = False

        if tool_name == "start_task":
            scenario = tool_args.get("scenario", "default")
            result_content = str(bolt_wrapper.start(scenario))
        elif tool_name == "poll_insights":
            result = bolt_wrapper.poll()
            if isinstance(result, dict):
                result_content = json.dumps(result, indent=2)
            else:
                result_content = str(result)
        elif tool_name == "cancel_task":
            result = bolt_wrapper.cancel()
            result_content = str(result)
        else:
            result_content = f"Unknown tool: {tool_name}"
            is_error = True

        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "content": [{"type": "text", "text": result_content}],
                "isError": is_error
            }
        }

    # Respond to unhandled methods
    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {
            "code": -32601,
            "message": "Method not found"
        }
    }

def main():
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            req = json.loads(line)
            res = process_request(req)
            if res is not None:
                sys.stdout.write(json.dumps(res) + "\n")
                sys.stdout.flush()
        except json.JSONDecodeError:
            pass # Ignore malformed json

if __name__ == "__main__":
    main()
