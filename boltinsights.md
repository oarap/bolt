# Bolt Agent Insights: Project Description and Implementation Plan

## Project Name
Bolt Agent Insights

## Category
Data for AI

## Project Description
As AI agents increasingly become the primary users of data systems, databases and execution engines need to evolve from being human-observable systems into agent-operable systems. Today, when a query runs slowly or behaves unexpectedly, a human typically opens a query profile, inspects runtime metrics, identifies issues such as poor filter selectivity, spilling, join stalls, backpressure, or I/O bottlenecks, and then decides whether to cancel the query, wait for it to finish, or rerun a rewritten version. In an agent-first future, that same feedback loop should be directly available to software agents.

This project proposes adding an agent-facing runtime insight layer to Bolt. Instead of exposing only low-level execution stats or post-hoc query profiles, Bolt would produce structured, machine-actionable insights while a query is still in flight. These insights would summarize what the engine is seeing at runtime and provide enough context for an agent to make decisions. For example, Bolt could detect that a scan predicate is not selective, that a query is spending most of its time blocked on I/O, that an operator is spilling to disk, that output buffers are backpressuring upstream work, or that the query appears stalled.

The key value is not only visibility, but actionability. An agent connected to Bolt through a lightweight control surface could poll or subscribe to query insights and decide whether to continue execution, cancel the query, or issue a better query. This turns Bolt from a passive execution engine into an active partner in agentic workflows.

This project fits the "Data for AI" category because it asks a fundamental question: if databases are increasingly serving AI agents rather than only humans, what information should they expose, and in what format, so those agents can operate effectively? The answer is likely not dashboards, logs, or verbose profiles designed for humans. The answer is compact, structured runtime insight primitives that agents can consume programmatically.

Bolt is a strong foundation for this idea because much of the underlying signal already exists. Bolt already tracks task-level and operator-level runtime statistics, including output buffering, driver blocking, memory use, spill behavior, and rich scan metrics. That means the implementation work is not about inventing instrumentation from scratch. It is about translating existing execution telemetry into a higher-level insight layer designed for agent consumption.

## Problem Statement
Today, query execution systems mostly expose one of two things:

- raw runtime counters that are difficult for agents to interpret directly
- human-oriented query profiles that are only useful after the fact

This creates a gap for agentic systems:

- agents cannot easily tell whether a running query is healthy
- agents cannot tell whether a query is likely wasting time or resources
- agents cannot react early enough to cancel or improve a query
- the engine does not explain its own runtime behavior in a machine-consumable way

Bolt Agent Insights closes that gap by adding a structured runtime explanation layer on top of existing Bolt execution metrics.

## Core Product Idea
Bolt should generate runtime insights inside the engine while a query is running. These insights should be structured, stable, and easy for an agent to consume programmatically.

Instead of only exposing raw stats, Bolt should produce events like:

- `scan_low_selectivity`
- `scan_io_bound`
- `spill_detected`
- `output_backpressure`
- `query_stalled`
- `eta_band` (optional and best-effort only)

An agent can then use these events to decide whether to:

- continue the query
- cancel the query
- rerun a better version
- surface a warning to a human
- suppress or down-rank low-value insights in future runs

## Non-Goals
This project is not:

- a generic observability dashboard
- a full autonomous query rewrite system
- a cluster-wide distributed query intelligence platform in v1
- a brand-new telemetry stack
- an LLM-dependent execution layer

This project is:

- a Bolt-native runtime insight layer
- task-scoped first, query-scoped later
- rule-based in v1
- agent-facing first, human-facing second

## Design Principles
- Generate insights inside Bolt, not outside Bolt.
- Keep insight logic separate from operators and connectors.
- Reuse existing Bolt metrics rather than adding broad new instrumentation.
- Build the first version around a live Bolt `Task`.
- Keep the first version task-scoped and deterministic.
- Make cancel the only required control action in v1.
- Treat ETA as optional and explicitly uncertain.
- Optimize for a strong demo and clean integration boundaries.

## Development Methodology
This project should follow strict TDD development from the beginning.

Recommended workflow:

1. Write the unit tests first for the smallest behavior being introduced.
2. Write the integration tests first for the full execution path affected by that behavior.
3. Run the tests and confirm they fail for the expected reason.
4. Implement the minimum amount of production code needed to make the tests pass.
5. Refactor only after the test suite is green.
6. Do not add feature code without either unit coverage, integration coverage, or both.

Practical guidance for this project:

- start every phase by creating the failing tests before writing production code
- prefer rule-level unit tests for decision logic
- prefer task-level integration tests for live sampling and runtime behavior
- keep JSON schema tests stable so integrations do not drift
- treat the standalone demo as an integration target, not as a substitute for tests

## Delivery and Commit Policy
Implementation should follow a strict commit discipline in addition to TDD.

Rules:

- every subphase must end with its own git commit
- every phase must end with a separate phase-completion git commit
- every commit must leave the repository in a buildable and runnable state
- no commit should be created until the relevant unit and integration tests for that subphase are green
- phase-completion commits must only be created after the full phase test suite passes
- do not batch multiple unfinished subphases into one commit
- do not create "WIP" or broken commits

Definition of done for any subphase commit:

- the code builds successfully
- the newly added or modified tests pass
- the implementation for that subphase is complete, not partial
- interfaces and behavior are documented well enough for the next subphase

Definition of done for any phase-completion commit:

- all subphase commits for that phase are already complete
- the entire phase builds cleanly
- the targeted test suite for the entire phase passes
- the code is in good integration shape for the next phase
- the phase can be demoed at its intended scope

Recommended execution cadence:

1. Write failing tests for the next subphase.
2. Implement only what is needed to make that subphase pass.
3. Run the targeted build and test commands.
4. Create the subphase commit only after the repository is green.
5. After all subphases in a phase are complete, run the full phase validation.
6. Create the phase-completion commit only after the phase is stable end to end.

## Where Insights Are Generated
Insights should be generated inside Bolt, in a new reusable module under `bolt/exec/insights/`.

The generation layer should sit above operators and task metrics:

- operators and connectors produce raw metrics
- `Task` aggregates runtime state
- the insight session polls the live task
- the insight engine evaluates rules
- Bolt emits structured events
- external agents or integrations consume them

This keeps the core intelligence in Bolt while allowing different integration surfaces later.

## When Insights Are Generated
There are three insight phases.

### Preflight Phase
Generated once after the plan exists but before execution begins.

Use this only for coarse hints such as:

- likely full scan
- no clearly selective predicate
- potentially expensive operator mix

This phase is optional for the MVP.

### Runtime Phase
Generated repeatedly while the task is running.

Every `250-500ms`:

- sample `task->taskStats()`
- derive plan-node stats
- compare against previous samples
- evaluate insight rules
- emit `new`, `updated`, or `resolved` events

This is the main phase and the center of the project.

### Terminal Phase
Generated once when the task finishes, fails, or is canceled.

Use this to:

- resolve any still-open insights
- emit final task summary
- preserve final evidence for logs or agents

## Ownership and Lifecycle
The thing that should initiate the insight session is the same thing that creates the Bolt `Task`.

That means the lifecycle should look like this:

1. Create a Bolt `Task`.
2. Attach a `TaskInsightSession`.
3. Start task execution.
4. Poll and evaluate insights while the task runs.
5. Stop and close the session when the task completes.

The session should not be initiated by:

- individual operators
- connectors
- sink-format code
- an external agent
- an external transport layer

The natural hook is immediately after `Task::create(...)`.

## Recommended Naming
For scope clarity, the first implementation should be task-scoped.

Recommended v1 naming:

- `TaskInsightSession`
- `TaskSample`
- `TaskSampleCollector`
- `InsightRule`
- `InsightEngine`

Later, if needed, add:

- `QueryInsightCoordinator`

This avoids overpromising query-wide behavior in the first version.

## Existing Bolt Surfaces To Reuse
The project should be built on top of existing Bolt internals.

Primary sources:

- `bolt/exec/Task.h`
- `bolt/exec/Task.cpp`
- `bolt/exec/TaskStats.h`
- `bolt/exec/OperatorStats.h`
- `bolt/exec/PlanNodeStats.h`
- `bolt/exec/PlanNodeStats.cpp`
- `bolt/exec/TableScan.cpp`
- `bolt/connectors/hive/HiveDataSource.cpp`

Useful existing capabilities:

- live `taskStats()` snapshots
- task cancel and pause hooks
- output buffer utilization signals
- spill counters
- blocked driver information
- long-running operator call reporting
- plan-node rollups from task stats
- scan-level runtime metrics like `ioWaitWallNanos` and `totalScanTime`

## Proposed Module Layout

### Core Insight Module
Create a new directory:

`bolt/exec/insights/`

Suggested files:

- `Insight.h`
- `InsightOptions.h`
- `InsightEventJson.h`
- `InsightEventJson.cpp`
- `TaskSample.h`
- `TaskSampleCollector.h`
- `TaskSampleCollector.cpp`
- `InsightRule.h`
- `InsightEngine.h`
- `InsightEngine.cpp`
- `TaskInsightSession.h`
- `TaskInsightSession.cpp`

Suggested rules directory:

- `bolt/exec/insights/rules/ScanLowSelectivityRule.h`
- `bolt/exec/insights/rules/ScanLowSelectivityRule.cpp`
- `bolt/exec/insights/rules/ScanIoBoundRule.h`
- `bolt/exec/insights/rules/ScanIoBoundRule.cpp`
- `bolt/exec/insights/rules/SpillDetectedRule.h`
- `bolt/exec/insights/rules/SpillDetectedRule.cpp`
- `bolt/exec/insights/rules/OutputBackpressureRule.h`
- `bolt/exec/insights/rules/OutputBackpressureRule.cpp`
- `bolt/exec/insights/rules/QueryStalledRule.h`
- `bolt/exec/insights/rules/QueryStalledRule.cpp`

### Standalone Tool
Create a new directory:

`bolt/tool/insights/`

Suggested files:

- `BoltInsightsMain.cpp`
- `InsightRunner.h`
- `InsightRunner.cpp`
- `InsightDemoQueries.h`
- `InsightDemoQueries.cpp`
- `PolicyAgent.h`
- `PolicyAgent.cpp`

## Core Data Model

### Insight Event
Each insight should be emitted as structured JSON.

Recommended fields:

- `schema_version`
- `sequence_id`
- `event_time_ms`
- `query_id`
- `task_id`
- `kind`
- `phase`
- `state`
- `severity`
- `confidence`
- `scope`
- `evidence`
- `recommended_actions`
- `human_summary`

### Insight State
Allowed values:

- `new`
- `updated`
- `resolved`

### Insight Phase
Allowed values:

- `preflight`
- `runtime`
- `terminal`

### Severity
Allowed values:

- `info`
- `low`
- `medium`
- `high`

### Scope
Recommended scope payload:

- `plan_node_id`
- `operator_type`
- `pipeline_id`
- `driver_group`
- `task_id`

### Evidence
Evidence should be a numeric or structured JSON object that explains why the insight exists.

Example evidence:

- `raw_input_rows`
- `output_rows`
- `pass_through_ratio`
- `spilled_bytes`
- `output_buffer_utilization`
- `io_wait_ratio`
- `stalled_samples`

### Recommended Actions
Keep actions coarse and stable:

- `continue`
- `cancel_query`
- `rerun_with_better_filter`
- `rerun_with_smaller_result`
- `inspect_spill_configuration`

## Session API
A first-pass C++ API can look like this:

```cpp
class TaskInsightSession {
 public:
  static std::shared_ptr<TaskInsightSession> attach(
      const std::shared_ptr<exec::Task>& task,
      InsightOptions options = {},
      SessionMetadata metadata = {});

  QuerySnapshot snapshot() const;
  std::vector<InsightEvent> poll();
  bolt::ContinueFuture cancel();
  void close();
};
```

This is enough for:

- standalone demo
- internal framework integration
- future transport wrappers

## Snapshot Model
The session should expose a lightweight task snapshot separate from insights.

Recommended snapshot fields:

- `query_id`
- `task_id`
- `task_state`
- `start_time_ms`
- `elapsed_ms`
- `output_buffer_utilization`
- `num_total_drivers`
- `num_running_drivers`
- `num_blocked_drivers`
- `spilled_bytes`
- `raw_input_rows`
- `output_rows`
- `open_insight_count`
- `open_insight_kinds`

## Rule Design
Every rule should consume at least:

- current sample
- previous sample
- optional recent sample window
- rule options

Every rule should produce either:

- no event
- a `new` event
- an `updated` event
- a `resolved` event

Every rule should also have:

- warmup requirements
- minimum evidence threshold
- cooldown and anti-flap behavior
- confidence assignment logic

## First-Pass Rule Set

### 1. `spill_detected`
Purpose:
Detect when a task or plan-node has started spilling.

Signals:

- `spilledBytes > 0`

Why it is good:

- high confidence
- easy to explain
- very demo-friendly

Recommended action:

- `continue`
- `cancel_query`
- `inspect_spill_configuration`

Confidence:

- very high

### 2. `output_backpressure`
Purpose:
Detect when downstream output buffering is slowing or blocking execution.

Signals:

- `outputBufferUtilization`
- `outputBufferOverutilized`

Why it is good:

- clear operational meaning
- strongly action-oriented
- useful for both agents and humans

Recommended action:

- `continue`
- `cancel_query`
- `rerun_with_smaller_result`

Confidence:

- high

### 3. `scan_low_selectivity`
Purpose:
Detect when a scan predicate is not filtering much data.

Signals:

- `rawInputRows`
- `outputRows`
- pass-through ratio

Example rule:
Emit if `outputRows / rawInputRows > 0.85` after warmup and minimum rows.

Recommended action:

- `continue`
- `rerun_with_better_filter`
- `cancel_query`

Confidence:

- medium to high depending on sample size

### 4. `scan_io_bound`
Purpose:
Detect when scan work is dominated by I/O wait instead of CPU progress.

Signals:

- `ioWaitWallNanos`
- `totalScanTime`
- optional read-byte metrics

Example rule:
Emit if I/O wait dominates scan time over multiple consecutive samples.

Recommended action:

- `continue`
- `inspect_data_layout`
- `rerun_with_better_filter`

Confidence:

- medium

### 5. `query_stalled`
Purpose:
Detect when the task is making little or no progress over multiple polls.

Signals:

- no meaningful row-progress delta
- blocked driver counts
- `longestRunningOpCallMs`

Example rule:
Emit only after several consecutive samples with no progress.

Recommended action:

- `continue`
- `cancel_query`

Confidence:

- medium

## Rules To Delay
These are good ideas but should not be in the first hackathon cut:

- join build and probe waiting diagnosis
- skew detection
- memory pressure prediction
- query rewrite suggestions inside Bolt
- cluster-wide ETA
- advanced per-operator bottleneck ranking

## Polling Strategy
Use polling, not a complex event bus.

Recommended defaults:

- poll every `250ms` for demos
- poll every `500ms` for general use
- keep last `10-20` samples
- require `2-3` samples before rule evaluation
- use stronger evidence thresholds after startup

Why polling is the right MVP:

- Bolt already supports live stats reads
- implementation is straightforward
- stable for demos
- avoids inventing a new progress event mechanism

## Insight Deduplication and Resolution
The engine should not emit the same alert as a new insight over and over.

It should:

- assign a stable insight key
- update an existing insight when evidence changes
- resolve an insight when the triggering condition clears
- keep a small open-insight map per session

Recommended insight key shape:

- `kind + scope + task_id`

## Implementation Phases

## Phase 1: Bolt Core Insight Engine
Estimated effort: `3-4 days`

### Goal
Build the reusable Bolt-native runtime insight layer.

### Subphases and Commit Boundaries

#### Phase 1.1: Core Data Model and Failing Tests
Commit requirement:

- create the unit and integration tests first for insight event types, snapshot types, and session metadata propagation
- add only the minimum scaffolding needed to compile the tests
- commit only when the repository builds and the intentionally scoped tests pass for the completed scaffolding layer

#### Phase 1.2: Sampling and Session Lifecycle
Commit requirement:

- create the failing tests first for `TaskSampleCollector`, `TaskInsightSession`, attach and close behavior, and cancel delegation
- implement the sampling loop and session lifecycle
- commit only when the relevant unit and integration tests are green and the repository remains buildable

#### Phase 1.3: Engine Wiring and Serialization
Commit requirement:

- create the failing tests first for snapshot serialization, event serialization, and terminal-state handling
- implement JSON serialization, sequence ids, metadata propagation, and engine wiring
- commit only when the new tests are green and the repository builds cleanly

#### Phase 1 Completion Commit
Commit requirement:

- run the complete targeted Phase 1 test suite
- verify the core insight module can attach to a live Bolt task and emit stable snapshots
- create a separate phase-completion commit only after the full phase is stable

### Test Plan (Write First)
- Write unit tests first for `TaskSample`, `TaskSampleCollector`, session metadata propagation, and snapshot serialization behavior.
- Write unit tests first for session lifecycle behavior such as attach, close, terminal-state handling, and cancel delegation.
- Write integration tests first for live polling of a running Bolt `Task` using the existing task test patterns.
- Write integration tests first for terminal transitions: finished, canceled, and failed tasks.
- Write integration tests first for repeated polling stability so we know sampling does not interfere with execution.
- Run the tests before implementation and confirm they fail for the expected missing functionality.

### Deliverables
- `bolt/exec/insights/` module exists
- `TaskInsightSession` can attach to a live Bolt task
- sampling loop works
- snapshots work
- JSON serialization works
- cancel action works
- no framework-specific code

### Work Items
- Define core insight types.
- Define `InsightOptions`.
- Implement `TaskSample` and `TaskSampleCollector`.
- Implement `InsightRule` interface.
- Implement `InsightEngine`.
- Implement `TaskInsightSession`.
- Add schema versioning to emitted JSON.
- Add session metadata support for `query_id`, `task_id`, and optional external ids.
- Update build files to compile the new module.

### Exit Criteria
- one live Bolt task can be sampled safely
- snapshots are stable
- session shutdown is clean
- cancel works through the session API

## Phase 2: Rule Set, Standalone Tool, and Demo
Estimated effort: `2-3 days`

### Goal
Make the feature real and demoable without any framework dependency.

### Subphases and Commit Boundaries

#### Phase 2.1: Rule Tests and First Rule Set
Commit requirement:

- create the failing unit tests first for the first rule set
- implement `spill_detected`, `output_backpressure`, `scan_low_selectivity`, `scan_io_bound`, and `query_stalled`
- commit only when all rule tests are green and the repository still builds cleanly

#### Phase 2.2: Insight Engine State Transitions
Commit requirement:

- create the failing tests first for deduplication, update, cooldown, and resolution behavior
- implement open-insight tracking and stable key handling
- commit only when these tests pass and the code remains buildable

#### Phase 2.3: Standalone Tool and Policy Agent
Commit requirement:

- create the failing integration tests first for `run`, `snapshot`, `stream`, `cancel`, and policy-driven cancel behavior
- implement the standalone tool and the deterministic policy agent
- commit only when the CLI and end-to-end tests pass and the repository builds successfully

#### Phase 2 Completion Commit
Commit requirement:

- run the complete targeted Phase 2 test suite
- validate the demo path end to end
- create a separate phase-completion commit only after the phase is stable and demoable

### Test Plan (Write First)
- Write unit tests first for each rule with synthetic samples covering positive, negative, warmup, and cooldown cases.
- Write unit tests first for deduplication, update, and resolution behavior in the `InsightEngine`.
- Write unit tests first for confidence assignment and evidence payload shape so the JSON contract stays predictable.
- Write integration tests first for end-to-end event generation from a live Bolt task through the standalone session API.
- Write integration tests first for the CLI commands: `run`, `snapshot`, `stream`, and `cancel`.
- Write integration tests first for the built-in policy agent, including cancel-on-insight behavior.
- Run the tests before implementation and confirm they fail for the expected missing functionality.

### Deliverables
- first 5 rules implemented
- standalone CLI and tool under `bolt/tool/insights/`
- JSONL event stream
- built-in deterministic policy mode
- one or two reliable demo scenarios

### Work Items
- implement the first five rules
- add anti-flap logic
- add confidence calculation
- add human-readable summaries
- build `bolt_insights` CLI
- add `run`, `snapshot`, `stream`, and `cancel` commands
- add a simple rule-based policy agent
- create demo inputs or queries
- make the demo deterministic enough for presentation

### Exit Criteria
- a running Bolt query emits live insight events
- a policy consumer can cancel a query based on those events
- the demo works without any external framework integration

## Phase 3: Internal Presto-Style Integration
Estimated effort: `2-4 days`
Owner: internal implementation

### Goal
Wire the already-built Bolt insight layer into the internal native worker path that creates Bolt tasks.

### Subphases and Commit Boundaries

#### Phase 3.1: Session Registry and Metadata Mapping
Commit requirement:

- create the failing tests first for registry behavior and metadata mapping
- implement registry creation, lookup, and cleanup
- commit only when the registry tests pass and the integration build remains healthy

#### Phase 3.2: Task-Creation Hook and Retrieval Surface
Commit requirement:

- create the failing integration tests first for automatic session attach at task creation and for snapshot and event retrieval
- implement the hook and retrieval surface
- commit only when retrieval works end to end and the code builds cleanly

#### Phase 3.3: Cancel Path and Lifecycle Cleanup
Commit requirement:

- create the failing tests first for cancel propagation and terminal cleanup
- implement cancel flow and cleanup semantics
- commit only when the tests pass and the integration remains runnable

#### Phase 3 Completion Commit
Commit requirement:

- run the complete targeted Phase 3 test suite
- verify the internal worker path can create, retrieve, and cancel insight sessions reliably
- create a separate phase-completion commit only after the full phase is stable

### Test Plan (Write First)
- Write unit tests first for the integration registry that maps external query and task identifiers to `TaskInsightSession` instances.
- Write unit tests first for lifecycle cleanup logic so sessions are removed on completion, failure, cancellation, and shutdown.
- Write unit tests first for metadata mapping so external query ids, task ids, stage ids, and fragment ids are attached correctly.
- Write integration tests first for task creation followed by automatic `TaskInsightSession::attach(...)`.
- Write integration tests first for live snapshot retrieval through the internal control surface.
- Write integration tests first for live event retrieval through the internal control surface.
- Write integration tests first for cancel flowing from the external control path into the underlying Bolt task.
- Run the tests before implementation and confirm they fail for the expected missing integration hooks.

### Assumption
The internal adapter already creates Bolt tasks and already manages task lifecycle, status, and cancellation.

### Deliverables
- insight session initiated at task creation time
- session registry keyed by external query and task ids
- ability to fetch snapshots
- ability to fetch insight events
- ability to cancel Bolt task from the external control plane

### Integration Responsibilities
- call `TaskInsightSession::attach(...)` immediately after creating the Bolt `Task`
- propagate external identifiers into session metadata
- register the session in a lifecycle-managed registry
- expose a control surface for insight polling and cancel
- clean up sessions on task completion or worker shutdown

### Recommended Metadata
- external query id
- external task id
- stage or fragment id
- SQL text or SQL hash if safe
- user or tenant if available
- submit time

### Exit Criteria
- internal worker can create and track sessions for live Bolt tasks
- external control plane can retrieve live Bolt insights
- cancel action works through the integrated path

## Phase 4: Internal Gluten-Style Integration
Estimated effort: `3-6 days`
Owner: internal implementation

### Goal
Wire the same Bolt insight layer into the internal Spark-native backend path.

### Subphases and Commit Boundaries

#### Phase 4.1: Native Session Registration and Spark Metadata Mapping
Commit requirement:

- create the failing tests first for executor-local registration and Spark metadata propagation
- implement the metadata and registry layer
- commit only when the tests pass and the integration build remains healthy

#### Phase 4.2: Native Execution Hook and Insight Retrieval
Commit requirement:

- create the failing integration tests first for automatic session creation during native execution setup and for snapshot and event retrieval
- implement the hook and the retrieval bridge
- commit only when these tests pass and the integration remains buildable and runnable

#### Phase 4.3: Cancel Flow and Executor-Local Cleanup
Commit requirement:

- create the failing tests first for cancel propagation and cleanup behavior
- implement cancel flow and shutdown cleanup
- commit only when the tests pass and the integration stays stable

#### Phase 4 Completion Commit
Commit requirement:

- run the complete targeted Phase 4 test suite
- verify the executor-local integration path works end to end
- create a separate phase-completion commit only after the full phase is stable

### Test Plan (Write First)
- Write unit tests first for native-side session registration and lookup keyed by Spark-side execution metadata.
- Write unit tests first for metadata propagation so Spark query id, stage id, task attempt id, partition id, and native task id are all preserved correctly.
- Write unit tests first for cleanup behavior when executor-local native execution is closed early or fails.
- Write integration tests first for native execution creation followed by automatic `TaskInsightSession` attachment.
- Write integration tests first for snapshot retrieval from the external or JNI-facing bridge.
- Write integration tests first for event retrieval from the external or JNI-facing bridge.
- Write integration tests first for cancel flowing from the integration bridge down to the underlying Bolt task.
- Write one executor-local end-to-end integration test first for the full path: create native execution, read insights, cancel, and verify cleanup.
- Run the tests before implementation and confirm they fail for the expected missing integration hooks.

### Assumption
The internal backend creates Bolt-backed execution kernels or Bolt tasks and already has a way to correlate Spark execution with native execution.

### Deliverables
- session initiated during native execution creation
- executor-local session registry
- external or JNI-accessible snapshot retrieval
- external or JNI-accessible event retrieval
- cancel support

### Integration Responsibilities
- initiate `TaskInsightSession` at native execution creation time
- attach Spark-side identifiers to session metadata
- expose polling methods for snapshots and events
- ensure executor-local cleanup
- keep the first version executor-local and task-scoped

### Recommended Metadata
- Spark query id if available
- Spark stage id
- Spark task attempt id
- partition id
- native task id
- SQL hash or plan hash

### Exit Criteria
- executor-local native execution can surface Bolt insights
- an internal agent can consume them
- cancel works end to end

## Effort Summary
If Phase 1 and Phase 2 are done cleanly as reusable Bolt core plus tooling, then either Phase 3 or Phase 4 is mostly integration plumbing.

Estimated effort by phase:

- Phase 1: `3-4 days`
- Phase 2: `2-3 days`
- Phase 3: `2-4 days`
- Phase 4: `3-6 days`

Estimated total for a hackathon-quality standalone deliverable:

- `5-7 days`

Estimated extra work to integrate into one internal framework path after that:

- `30-60%` additional effort if the standalone version is cleanly factored

## Demo Plan

### Demo Story A: Advisory Insight
- run a query with weak filtering
- Bolt emits `scan_low_selectivity`
- the agent logs the warning but allows execution to continue
- Bolt finishes successfully

### Demo Story B: Corrective Action
- run a query that triggers backpressure or spill
- Bolt emits `output_backpressure` or `spill_detected`
- the agent decides to cancel
- the agent reruns a better query
- show improved behavior

### Demo Story C: Internal Integration Demo
- create a task through the internal framework path
- session starts automatically
- the agent retrieves snapshots and live insight events
- the agent issues cancel through the integrated control path

## Success Criteria
This project is successful if:

- Bolt can produce structured runtime insights from a live task
- those insights are grounded in real Bolt execution data
- an agent can consume them without parsing logs or UI text
- Bolt can expose a usable control loop centered on insight-driven cancel or rerun
- the same core logic can be reused in standalone mode and internal framework integrations

## Risks
- trying to make v1 query-scoped instead of task-scoped
- spending too much time on transport before core logic works
- building too many rules
- overpromising ETA accuracy
- relying on an LLM for core functionality
- making the demo depend on flaky runtime conditions

## Risk Mitigations
- keep v1 task-scoped
- build rules that use high-confidence existing signals
- use deterministic demo scenarios
- make the built-in policy agent rule-based
- freeze the JSON schema before starting integrations
- keep cancel as the only required control action

## Things Explicitly Out of Scope for Hackathon
- in-flight query rewriting inside Bolt
- cluster-wide distributed query coordinator
- broad external service platform
- dozens of insight categories
- autonomous SQL optimization engine
- polished dashboard-first UX

## Recommended Build Order
1. Phase 1
2. Phase 2
3. Freeze APIs and JSON schema.
4. Implement Phase 3 internally or Phase 4 internally.
5. Add optional stretch work only if time remains.

## Stretch Goals
- preflight hints
- pause and resume support
- ETA confidence band
- compact human-readable terminal UI
- insight suppression preferences
- lightweight query-level coordinator

## Final Recommendation
For hackathon execution, the best path is:

- build the reusable Bolt-native core first
- build the standalone demo second
- treat internal integrations as adapters on top of a stable Bolt API
- keep the story focused on agent control loops, not dashboards

That gives the project a strong architecture, a believable demo, and a realistic path from hackathon prototype to internal adoption.
