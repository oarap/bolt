import json
import sys
import os
import subprocess
import threading
import time

class ShowcaseBoltInsightsWrapper:
    def __init__(self):
        self.process = None
        self.output_thread = None

    def start(self, scenario):
        if self.process is not None:
            self.stop()
        
        bolt_exec = os.path.expanduser("~/bolt/_build/Debug/bolt/tool/insights/bolt_insights_tool")
        
        self.process = subprocess.Popen(
            [bolt_exec, "--command=interactive", f"--scenario={scenario}"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1 # Line buffered
        )

        try:
            line = self.process.stdout.readline()
            if "ready" not in line:
                return f"Failed to start properly: {line}"
        except Exception as e:
            return f"Error starting: {e}"

        return f"Successfully started showcase scenario: {scenario}"

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

    def take_action(self, action_type):
        if self.process is None or self.process.poll() is not None:
            return "No active session."
            
        if action_type == "cancel":
            try:
                self.process.stdin.write("cancel\n")
                self.process.stdin.flush()
                line = self.process.stdout.readline()
                if line:
                    return json.loads(line)
                return "Sent cancel request."
            except Exception as e:
                return {"error": f"Cancel failed: {e}"}
        elif action_type == "rerun":
            self.stop()
            return {"status": "rerun_initiated", "message": "Task killed. Ready to rewrite plan and rerun."}
        elif action_type == "continue":
            return {"status": "continuing", "message": "Ignored insight, continuing execution."}
        else:
            return {"error": f"Unknown action: {action_type}"}

bolt_wrapper = ShowcaseBoltInsightsWrapper()

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
                "serverInfo": {"name": "bolt-insights-showcase", "version": "2.0.0"}
            }
        }
        
    elif method == "tools/list":
        return {
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "tools": [
                    {
                        "name": "start_scenario",
                        "description": "Start a specific Bolt Insights showcase scenario.",
                        "inputSchema": {
                            "type": "object",
                            "properties": {
                                "scenario": {
                                    "type": "string", 
                                    "description": "The scenario to run. Options: 'default' (Query Stalled), 'mock_backpressure' (Output Backpressure), 'mock_low_selectivity' (Scan Low Selectivity), 'mock_spill_detected' (Spill Detected)"
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
                        "name": "take_action",
                        "description": "Take an autonomous action based on the polled insights.",
                        "inputSchema": {
                            "type": "object",
                            "properties": {
                                "action": {
                                    "type": "string",
                                    "description": "Action to take. Options: 'cancel' (kill bad query), 'rerun' (kill and rewrite query), 'continue' (ignore and let it run)"
                                }
                            },
                            "required": ["action"]
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

        if tool_name == "start_scenario":
            scenario = tool_args.get("scenario", "default")
            result_content = str(bolt_wrapper.start(scenario))
        elif tool_name == "poll_insights":
            result = bolt_wrapper.poll()
            if isinstance(result, dict):
                result_content = json.dumps(result, indent=2)
            else:
                result_content = str(result)
        elif tool_name == "take_action":
            action = tool_args.get("action", "continue")
            result = bolt_wrapper.take_action(action)
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
