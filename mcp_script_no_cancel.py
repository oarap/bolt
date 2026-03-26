import sys
import json
import time
from mcp_server import bolt_wrapper

def run_demo():
    print("Starting task...")
    res = bolt_wrapper.start("default")
    print(f"Start result: {res}")
    
    max_polls = 10
    
    for i in range(1, max_polls + 1):
        time.sleep(0.5)
        print(f"\nPolling insights (attempt {i})...")
        poll_res = bolt_wrapper.poll()
        print(f"Poll {i} result: {json.dumps(poll_res, indent=2)}")
        
        if poll_res.get("state") in ["Finished", "Canceled", "Failed"]:
            print(f"\nTask terminal state reached: {poll_res.get('state')}")
            break

if __name__ == "__main__":
    run_demo()
