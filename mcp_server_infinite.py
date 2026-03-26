import sys
import json
import os
import subprocess
import threading
import time

TOOL_PATH = "/home/omer.arap/bolt/_build/Debug/bolt/tool/insights/bolt_insights_tool"
INSIGHTS_FILE = "/tmp/bolt_insights.json"

class InsightServer:
    def __init__(self):
        self.process = None

    def start(self, scenario):
        if scenario == "spark":
            # Clear previous insights
            if os.path.exists(INSIGHTS_FILE):
                os.remove(INSIGHTS_FILE)
            return "Started monitoring Spark insights (via file)"
            
        if self.process:
            if self.process.poll() is None:
                self.stop_server()
            
        # Start the bolt_insights_tool as a subprocess
        cmd = [TOOL_PATH, "--command=interactive", f"--scenario={scenario}"]
        self.process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1 # Line buffered
        )
        # Read the initial "ready" prompt
        out = self.process.stdout.readline()
        return f"Successfully started scenario: {scenario}"

    def poll(self):
        # Always check the file first for Spark insights
        spark_events = []
        if os.path.exists(INSIGHTS_FILE):
            try:
                with open(INSIGHTS_FILE, 'r') as f:
                    for line in f:
                        if line.strip():
                            spark_events.append(json.loads(line))
                # Clear file after reading
                open(INSIGHTS_FILE, 'w').close()
            except Exception as e:
                pass
                
        if spark_events:
            return {"events": spark_events, "state": "Running (Spark)"}

        if not self.process or self.process.poll() is not None:
            return {"error": "No active session."}
        
        self.process.stdin.write("poll\n")
        self.process.stdin.flush()
        
        response = self.process.stdout.readline().strip()
        try:
            return json.loads(response)
        except:
            return {"error": "Failed to parse response", "raw": response}

    def cancel(self):
        if not self.process or self.process.poll() is not None:
            return "No task to cancel"
            
        self.process.stdin.write("cancel\n")
        self.process.stdin.flush()
        
        response = self.process.stdout.readline().strip()
        try:
            return json.loads(response)
        except:
            return {"status": "canceled", "raw": response}

    def stop_server(self):
        if self.process and self.process.poll() is None:
            try:
                self.process.stdin.write("exit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=2)
            except:
                self.process.terminate()
            self.process = None
        return "Server shutting down..."

bolt_wrapper = InsightServer()

def process_request(req):
    method = req.get("method")
    params = req.get("params", {})
    req_id = req.get("id")

    if method == "initialize":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "bolt-insights-server", "version": "2.0.0"}
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
                        "description": "Start a specific Bolt Insights showcase scenario.",
                        "inputSchema": {
                            "type": "object",
                            "properties": {
                                "scenario": {
                                    "type": "string", 
                                    "description": "The scenario to run. Options: 'default' (Query Stalled), 'spark' (Monitor real spark executions), 'mock_backpressure', 'mock_low_selectivity', 'mock_spill_detected'"
                                }
                            },
                            "required": ["scenario"]
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
                        "description": "Cancel the running task.",
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
            if isinstance(result, dict):
                result_content = json.dumps(result, indent=2)
            else:
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

    return {
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": -32601, "message": "Method not found"}
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
            pass

if __name__ == "__main__":
    main()
