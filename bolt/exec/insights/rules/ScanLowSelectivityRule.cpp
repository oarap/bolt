#include "bolt/exec/insights/rules/ScanLowSelectivityRule.h"
#include <folly/json.h>

namespace bytedance::bolt::exec::insights::rules {

std::optional<InsightEvent> ScanLowSelectivityRule::evaluate(
    const TaskSampleCollector& collector,
    const InsightOptions& options) {
  auto latestOpt = collector.getLatestSample();
  if (!latestOpt) {
    return std::nullopt;
  }
  
  const auto& latest = *latestOpt;

  for (const auto& pipeline : latest.taskStats.pipelineStats) {
    if (!pipeline.inputPipeline) continue;
    
    for (const auto& op : pipeline.operatorStats) {
      if (op.operatorType == "TableScan") {
        uint64_t in = op.rawInputPositions;
        uint64_t out = op.outputPositions;
        
        uint64_t minRows = options.minRowsForSelectivity > 0 ? options.minRowsForSelectivity : 10000;
        double threshold = options.lowSelectivityThreshold > 0.0 ? options.lowSelectivityThreshold : 0.95;
        
        // Need minimum rows to judge
        if (in > minRows) {
          double passThroughRatio = static_cast<double>(out) / in;
          
          if (passThroughRatio > threshold && reportedNodes_.find(op.planNodeId) == reportedNodes_.end()) {
            reportedNodes_.insert(op.planNodeId);
            
            InsightEvent event;
            event.eventTimeMs = latest.timestampMs;
            event.kind = name();
            event.phase = InsightPhase::kRuntime;
            event.state = InsightState::kNew;
            event.severity = InsightSeverity::kMedium;
            event.confidence = "high";
            
            event.scope.planNodeId = op.planNodeId;
            event.scope.operatorType = op.operatorType;
            
            event.evidence = folly::dynamic::object
              ("raw_input_rows", in)
              ("output_rows", out)
              ("pass_through_ratio", passThroughRatio);
              
            event.recommendedActions = {"continue", "rerun_with_better_filter"};
            event.humanSummary = "Table scan has low selectivity (passing through " + 
                                  std::to_string(static_cast<int>(passThroughRatio * 100)) + "%)";
            
            return event;
          }
        }
      }
    }
  }

  return std::nullopt;
}

} // namespace bytedance::bolt::exec::insights::rules
