#pragma once

#include <optional>
#include "bolt/exec/insights/Insight.h"
#include "bolt/exec/insights/InsightOptions.h"
#include "bolt/exec/insights/TaskSampleCollector.h"

namespace bytedance::bolt::exec::insights {

class InsightRule {
 public:
  virtual ~InsightRule() = default;

  virtual std::string name() const = 0;
  
  // Evaluates the rule and returns an InsightEvent if a condition is met
  // This might be a new event, an update, or a resolution
  virtual std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) = 0;
};

} // namespace bytedance::bolt::exec::insights
