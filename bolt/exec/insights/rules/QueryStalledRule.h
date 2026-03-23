#pragma once

#include "bolt/exec/insights/InsightRule.h"

namespace bytedance::bolt::exec::insights::rules {

class QueryStalledRule : public InsightRule {
 public:
  std::string name() const override { return "query_stalled"; }

  std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) override;

 private:
  bool reported_{false};
};

} // namespace bytedance::bolt::exec::insights::rules
