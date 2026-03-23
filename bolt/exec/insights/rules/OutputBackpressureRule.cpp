#include "bolt/exec/insights/rules/OutputBackpressureRule.h"
#include <folly/json.h>

namespace bytedance::bolt::exec::insights::rules {

std::optional<InsightEvent> OutputBackpressureRule::evaluate(
    const TaskSampleCollector& collector,
    const InsightOptions& options) {
  auto latestOpt = collector.getLatestSample();
  if (!latestOpt) {
    return std::nullopt;
  }
  
  const auto& latest = *latestOpt;
  const auto& stats = latest.taskStats;

  if (stats.outputBufferOverutilized || stats.outputBufferUtilization > 0.9) {
    InsightEvent event;
    event.eventTimeMs = latest.timestampMs;
    event.kind = name();
    event.phase = InsightPhase::kRuntime;
    event.state = InsightState::kNew;
    event.severity = InsightSeverity::kMedium;
    event.confidence = "high";
    
    event.evidence = folly::dynamic::object
      ("output_buffer_utilization", stats.outputBufferUtilization)
      ("output_buffer_overutilized", stats.outputBufferOverutilized);
      
    event.recommendedActions = {"continue", "cancel_query"};
    event.humanSummary = "Output buffer is overutilized, indicating downstream backpressure";
    
    return event;
  }

  return std::nullopt;
}

} // namespace bytedance::bolt::exec::insights::rules
