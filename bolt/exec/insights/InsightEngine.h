#pragma once

#include <memory>
#include <vector>
#include "bolt/exec/insights/Insight.h"
#include "bolt/exec/insights/InsightOptions.h"
#include "bolt/exec/insights/InsightRule.h"
#include "bolt/exec/insights/TaskSampleCollector.h"

namespace bytedance::bolt::exec::insights {

class InsightEngine {
 public:
  InsightEngine();

  // Evaluates all registered rules against the collected samples
  // and returns a list of InsightEvents (new or updated).
  // Also handles resolution of open insights if they no longer apply.
  std::vector<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options);

  void addRule(std::unique_ptr<InsightRule> rule) {
    rules_.push_back(std::move(rule));
  }

  // Gets the kinds of currently open insights
  std::vector<std::string> getOpenInsightKinds() const;

  size_t getOpenInsightCount() const {
    return openInsights_.size();
  }

 private:
  std::vector<std::unique_ptr<InsightRule>> rules_;

  // Tracks currently open insights by a stable key (e.g. kind + node_id)
  std::unordered_map<std::string, InsightEvent> openInsights_;

  std::string generateKey(const InsightEvent& event) const;
};

} // namespace bytedance::bolt::exec::insights
