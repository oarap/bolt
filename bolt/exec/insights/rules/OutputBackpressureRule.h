#pragma once

#include "bolt/exec/insights/InsightRule.h"

namespace bytedance::bolt::exec::insights::rules {

class OutputBackpressureRule : public InsightRule {
 public:
  std::string name() const override { return "output_backpressure"; }

  std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) override;
};

} // namespace bytedance::bolt::exec::insights::rules
