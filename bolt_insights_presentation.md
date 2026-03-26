---
marp: true
theme: default
class: lead
paginate: true
backgroundColor: #ffffff
size: 16:9
---

# ⚡ Bolt Insights
## Intelligent Real-Time Execution Diagnostic Framework
---

# 1. The Core: Bolt Insights Generation

Bolt Insights operates as a real-time diagnostic layer deeply embedded within the Bolt/Velox execution engine.

- **`TaskInsightSession`:** Attaches to the lifecycle of a query task.
- **`TaskSampleCollector`:** Asynchronously polls raw C++ `exec::Task` stats (memory, drivers, operator metrics).
- **`InsightEngine`:** Evaluates a suite of rules against the sampled history to emit `InsightEvent`s.

---

# The 5 Native Insight Types

| Insight Type | Trigger Condition | Meaning |
| :--- | :--- | :--- |
| `query_stalled` | 0 drivers running for multiple samples | Query is deadlocked or starved. |
| `spill_detected` | In-memory buffers full, writing to disk | Memory-bound, performance will drop. |
| `scan_io_bound` | Table Scan blocked waiting for storage | Storage is slow or network saturated. |
| `output_backpressure`| Output buffers consistently >90% full | Client/network consuming too slowly. |
| `scan_low_selectivity`| Filter drops almost all read data | Missing partitions/indexes, bad plan. |

---

# Architecture Diagram: Core Generation

<div class="mermaid">
graph TD
    A[Query Task execution] -->|TaskStats| B(TaskSampleCollector)
    B -->|History Window| C{InsightEngine}
    C -->|Evaluates Rules| D[query_stalled]
    C -->|Evaluates Rules| E[spill_detected]
    C -->|Evaluates Rules| F[scan_io_bound]
    C -->|Evaluates Rules| G[output_backpressure]
    C -->|Evaluates Rules| H[scan_low_selectivity]
    
    style A fill:#f9f,stroke:#333,stroke-width:2px
    style C fill:#bbf,stroke:#333,stroke-width:2px
</div>

---

# 2. Execution Engines Integration

Bolt Insights monitors natively, regardless of the orchestrator.

**Presto Native (Presto C++)**
- `TaskManager` intercepts task creation.
- Passes Presto `queryId` and `taskId` to `InsightSessionRegistry`.
- Cleans up session upon task abort/finish.

**Spark & Gluten**
- Spark SQL offloads via Gluten JNI to `libbolt_backend.so`.
- Bolt reconstructs `exec::Task` in C++.
- Insights capture native metrics hidden from the JVM.

---

# Architecture Diagram: Engine Integration

<div class="mermaid">
graph LR
    subgraph Java/Scala Space
        Spark[Spark SQL] --> Gluten
        Presto[Presto Coordinator]
    end

    subgraph C++ Native Space
        Gluten -->|JNI| BoltBackend[libbolt_backend.so]
        Presto -->|REST/Thrift| PrestoWorker[Presto C++ TaskManager]
        
        BoltBackend --> Task[exec::Task]
        PrestoWorker --> Task
        
        Task --> Registry[InsightSessionRegistry]
        Registry --> Session[TaskInsightSession]
    end
    
    style Java/Scala Space fill:#fce4ec
    style C++ Native Space fill:#e3f2fd
</div>

---

# 3. Observation & Action Relaying

Bridging C++ metrics to external intelligence (LLMs/Agents).

### Observation Layers
1. **Standalone `bolt_insights_tool`:** Mocks execution for safe rule testing via `stdin/stdout`.
2. **File-based Export Bridge:** Real engines (Spark/Presto) write JSON to `/tmp/bolt_insights.json`.

### The Feedback Loop (Actions)
- LLMs poll the Python **MCP Server**.
- **Action:** Cancel Query or Recommend Config Changes.
- **Relay:** Uses `taskId` from JSON to hit Spark/Presto REST APIs to kill the job gracefully.

---

# Architecture Diagram: Observation & LLM Loop

<div class="mermaid">
sequenceDiagram
    participant LLM as LLM Agent (Claude)
    participant MCP as Python MCP Server
    participant Backend as Bolt C++ Backend
    participant Orchestrator as Spark / Presto
    
    Backend->>Backend: Detects spill_detected
    Backend-->>MCP: Writes to /tmp/bolt_insights.json
    LLM->>MCP: polls tools/call (poll_insights)
    MCP-->>LLM: Returns JSON (taskId: "123")
    LLM->>LLM: Analyzes severity (High)
    LLM->>Orchestrator: REST API: DELETE /v1/task/123
    Orchestrator->>Backend: Abort thread
</div>

---

# 🚀 Interactive Demo Guide

## Demo A: The Standalone Tool (The Action Loop)

**Setup:**
```bash
python3 ~/bolt/mcp_script_test.py
```
This script will:
1. Start the tool with the `default` scenario (query_stalled)
2. Poll insights
3. Detect the `query_stalled` state
4. Intervene by triggering a task cancellation

---

# 🚀 Interactive Demo Guide

## Demo B: The Spark Shell (Real Execution Insights)

**Setup:**
We use Gluten to offload Spark queries to the Bolt native backend:

```bash
bash ~/run_spark_with_gluten.sh
```

**What Happens Under the Hood:**
1. A massive Spark dataset (100M rows) is generated
2. A complex HashAggregation query is executed
3. `GlutenPlugin` intercepts the SQL plan and passes it via JNI to `libbolt_backend.so`
4. The C++ `TaskInsightSession` attaches to the native task
5. It begins analyzing memory, IO, and driver states inside the C++ engine!
