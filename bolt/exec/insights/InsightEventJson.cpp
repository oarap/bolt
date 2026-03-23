#include "bolt/exec/insights/InsightEventJson.h"
#include <folly/json.h>

namespace bytedance::bolt::exec::insights {

namespace {

std::string stateToString(InsightState state) {
  switch (state) {
    case InsightState::kNew:
      return "new";
    case InsightState::kUpdated:
      return "updated";
    case InsightState::kResolved:
      return "resolved";
    default:
      return "unknown";
  }
}

std::string phaseToString(InsightPhase phase) {
  switch (phase) {
    case InsightPhase::kPreflight:
      return "preflight";
    case InsightPhase::kRuntime:
      return "runtime";
    case InsightPhase::kTerminal:
      return "terminal";
    default:
      return "unknown";
  }
}

std::string severityToString(InsightSeverity severity) {
  switch (severity) {
    case InsightSeverity::kInfo:
      return "info";
    case InsightSeverity::kLow:
      return "low";
    case InsightSeverity::kMedium:
      return "medium";
    case InsightSeverity::kHigh:
      return "high";
    default:
      return "unknown";
  }
}

} // namespace

folly::dynamic toJson(const InsightScope& scope) {
  folly::dynamic obj = folly::dynamic::object;
  if (scope.planNodeId) {
    obj["plan_node_id"] = *scope.planNodeId;
  }
  if (scope.operatorType) {
    obj["operator_type"] = *scope.operatorType;
  }
  if (scope.pipelineId) {
    obj["pipeline_id"] = *scope.pipelineId;
  }
  if (scope.driverGroup) {
    obj["driver_group"] = *scope.driverGroup;
  }
  if (scope.taskId) {
    obj["task_id"] = *scope.taskId;
  }
  return obj;
}

folly::dynamic toJson(const InsightEvent& event) {
  folly::dynamic obj = folly::dynamic::object(
      "schema_version", event.schemaVersion)("sequence_id", event.sequenceId)(
      "event_time_ms", event.eventTimeMs)("kind", event.kind)(
      "phase", phaseToString(event.phase))("state", stateToString(event.state))(
      "severity", severityToString(event.severity))(
      "confidence", event.confidence)("human_summary", event.humanSummary);

  if (event.queryId) {
    obj["query_id"] = *event.queryId;
  }
  if (event.taskId) {
    obj["task_id"] = *event.taskId;
  }

  obj["scope"] = toJson(event.scope);
  obj["evidence"] = event.evidence;

  folly::dynamic actions = folly::dynamic::array;
  for (const auto& action : event.recommendedActions) {
    actions.push_back(action);
  }
  obj["recommended_actions"] = actions;

  return obj;
}

folly::dynamic toJson(const QuerySnapshot& snapshot) {
  folly::dynamic obj = folly::dynamic::object("task_state", snapshot.taskState)(
      "start_time_ms", snapshot.startTimeMs)("elapsed_ms", snapshot.elapsedMs)(
      "output_buffer_utilization", snapshot.outputBufferUtilization)(
      "num_total_drivers", snapshot.numTotalDrivers)(
      "num_running_drivers", snapshot.numRunningDrivers)(
      "num_blocked_drivers", snapshot.numBlockedDrivers)(
      "spilled_bytes", snapshot.spilledBytes)(
      "raw_input_rows", snapshot.rawInputRows)(
      "output_rows", snapshot.outputRows)(
      "open_insight_count", snapshot.openInsightCount);

  if (snapshot.queryId) {
    obj["query_id"] = *snapshot.queryId;
  }
  if (snapshot.taskId) {
    obj["task_id"] = *snapshot.taskId;
  }

  folly::dynamic kinds = folly::dynamic::array;
  for (const auto& kind : snapshot.openInsightKinds) {
    kinds.push_back(kind);
  }
  obj["open_insight_kinds"] = kinds;

  return obj;
}

} // namespace bytedance::bolt::exec::insights
