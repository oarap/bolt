#pragma once

#include "bolt/exec/insights/InsightRule.h"

namespace bytedance::bolt::exec::insights::rules {

class ScanIoBoundRule : public InsightRule {
 public:
  std::string name() const override { return "scan_io_bound"; }

  std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) override;

 private:
  std::unordered_set<std::string> reportedNodes_;
};

} // namespace bytedance::bolt::exec::insights::rules
