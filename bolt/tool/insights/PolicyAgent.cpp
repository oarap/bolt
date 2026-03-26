// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#include "bolt/tool/insights/PolicyAgent.h"

namespace bytedance::bolt::tool::insights {

PolicyAgent::PolicyDecision PolicyAgent::evaluate(
    const std::vector<exec::insights::InsightEvent>& events) {
  for (const auto& event : events) {
    if (event.state == exec::insights::InsightState::kResolved) {
      continue;
    }

    // Cancel on high severity backpressure or stall
    if (event.kind == "output_backpressure" &&
        event.severity >= exec::insights::InsightSeverity::kMedium) {
      return {Action::kCancel, "High severity backpressure detected"};
    }

    if (event.kind == "query_stalled" &&
        event.severity == exec::insights::InsightSeverity::kHigh) {
      return {Action::kCancel, "Query appears completely stalled"};
    }

    // Suggest rerun for low selectivity
    if (event.kind == "scan_low_selectivity" &&
        event.severity >= exec::insights::InsightSeverity::kMedium) {
      return {Action::kRerun, "Poor scan selectivity, consider adding filters"};
    }

    // Inspect spill
    if (event.kind == "spill_detected") {
      return {
          Action::kContinue,
          "Spill detected, performance might degrade. Monitoring..."};
    }
  }

  return {Action::kContinue, "Query is healthy"};
}

} // namespace bytedance::bolt::tool::insights
