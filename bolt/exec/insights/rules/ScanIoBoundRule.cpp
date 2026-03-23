#include "bolt/exec/insights/rules/ScanIoBoundRule.h"
#include <folly/json.h>

namespace bytedance::bolt::exec::insights::rules {

std::optional<InsightEvent> ScanIoBoundRule::evaluate(
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
        auto it = op.runtimeStats.find("ioWaitWallNanos");
        auto timeIt = op.runtimeStats.find("totalScanTime");
        
        if (it != op.runtimeStats.end() && timeIt != op.runtimeStats.end()) {
          int64_t ioWait = it->second.sum;
          int64_t totalTime = timeIt->second.sum;
          
          if (totalTime > 1000000000) { // More than 1s of scan time
            double ioRatio = static_cast<double>(ioWait) / totalTime;
            
            if (ioRatio > 0.8 && reportedNodes_.find(op.planNodeId) == reportedNodes_.end()) {
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
                ("io_wait_nanos", ioWait)
                ("total_scan_time_nanos", totalTime)
                ("io_wait_ratio", ioRatio);
                
              event.recommendedActions = {"continue", "cancel_query"};
              event.humanSummary = "Table scan is I/O bound (spending " + 
                                    std::to_string(static_cast<int>(ioRatio * 100)) + "% of time waiting for I/O)";
              
              return event;
            }
          }
        }
      }
    }
  }

  return std::nullopt;
}

} // namespace bytedance::bolt::exec::insights::rules
