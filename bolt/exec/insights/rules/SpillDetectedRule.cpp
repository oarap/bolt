#include "bolt/exec/insights/rules/SpillDetectedRule.h"
#include <folly/json.h>

namespace bytedance::bolt::exec::insights::rules {

std::optional<InsightEvent> SpillDetectedRule::evaluate(
    const TaskSampleCollector& collector,
    const InsightOptions& options) {
  auto latestOpt = collector.getLatestSample();
  if (!latestOpt) {
    return std::nullopt;
  }
  
  const auto& latest = *latestOpt;

  for (const auto& pipeline : latest.taskStats.pipelineStats) {
    for (const auto& op : pipeline.operatorStats) {
      if (op.spilledBytes > 0) {
        std::string nodeId = op.planNodeId;
        if (seenPlanNodes_.find(nodeId) == seenPlanNodes_.end()) {
          seenPlanNodes_.insert(nodeId);

          InsightEvent event;
          event.eventTimeMs = latest.timestampMs;
          event.kind = name();
          event.phase = InsightPhase::kRuntime;
          event.state = InsightState::kNew;
          event.severity = InsightSeverity::kHigh;
          event.confidence = "high";
          
          event.scope.planNodeId = nodeId;
          event.scope.operatorType = op.operatorType;
          
          event.evidence = folly::dynamic::object("spilled_bytes", op.spilledBytes);
          event.recommendedActions = {"continue", "cancel_query", "inspect_spill_configuration"};
          event.humanSummary = "Spill detected on plan node " + nodeId + " (" + op.operatorType + ")";
          
          return event;
        }
      }
    }
  }

  return std::nullopt;
}

} // namespace bytedance::bolt::exec::insights::rules
