#include "bolt/exec/insights/rules/QueryStalledRule.h"
#include <folly/json.h>

namespace bytedance::bolt::exec::insights::rules {

std::optional<InsightEvent> QueryStalledRule::evaluate(
    const TaskSampleCollector& collector,
    const InsightOptions& options) {
  
  if (reported_) {
    return std::nullopt;
  }

  auto recent = collector.getRecentSamples();
  // Need at least 3 samples to determine stall
  if (recent.size() < 3) {
    return std::nullopt;
  }

  int stalledCount = 0;
  for (const auto& sample : recent) {
    const auto& stats = sample.taskStats;
    if (stats.numTotalDrivers > 0 && stats.numRunningDrivers == 0) {
      // Check if actually blocked, not just finished
      bool hasBlocked = false;
      for (const auto& [reason, count] : stats.numBlockedDrivers) {
        if (count > 0) {
          hasBlocked = true;
          break;
        }
      }
      
      // If there are uncompleted drivers but none running and some blocked
      if (stats.numCompletedDrivers + stats.numTerminatedDrivers < stats.numTotalDrivers && hasBlocked) {
        stalledCount++;
      }
    }
  }

  // If stalled for all recent samples
  if (stalledCount == recent.size()) {
    reported_ = true;
    const auto& latest = recent.back();
    
    InsightEvent event;
    event.eventTimeMs = latest.timestampMs;
    event.kind = name();
    event.phase = InsightPhase::kRuntime;
    event.state = InsightState::kNew;
    event.severity = InsightSeverity::kHigh;
    event.confidence = "high";
    
    event.evidence = folly::dynamic::object
      ("stalled_samples", stalledCount);
      
    event.recommendedActions = {"cancel_query"};
    event.humanSummary = "Query appears stalled (no drivers running for multiple consecutive samples)";
    
    return event;
  }

  return std::nullopt;
}

} // namespace bytedance::bolt::exec::insights::rules
