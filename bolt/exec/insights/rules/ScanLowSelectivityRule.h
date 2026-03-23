#pragma once

#include "bolt/exec/insights/InsightRule.h"
#include <unordered_set>

namespace bytedance::bolt::exec::insights::rules {

class ScanLowSelectivityRule : public InsightRule {
 public:
  std::string name() const override { return "scan_low_selectivity"; }

  std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) override;

 private:
  std::unordered_set<std::string> reportedNodes_;
};

} // namespace bytedance::bolt::exec::insights::rules
