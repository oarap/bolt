#pragma once

#include "bolt/exec/insights/InsightRule.h"
#include <string>
#include <unordered_set>

namespace bytedance::bolt::exec::insights::rules {

class SpillDetectedRule : public InsightRule {
 public:
  std::string name() const override { return "spill_detected"; }

  std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) override;

 private:
  std::unordered_set<std::string> seenPlanNodes_;
};

} // namespace bytedance::bolt::exec::insights::rules
