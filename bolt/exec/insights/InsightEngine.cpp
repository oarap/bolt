#include "bolt/exec/insights/InsightEngine.h"
#include "bolt/exec/insights/rules/OutputBackpressureRule.h"
#include "bolt/exec/insights/rules/QueryStalledRule.h"
#include "bolt/exec/insights/rules/ScanIoBoundRule.h"
#include "bolt/exec/insights/rules/ScanLowSelectivityRule.h"
#include "bolt/exec/insights/rules/SpillDetectedRule.h"

namespace bytedance::bolt::exec::insights {

InsightEngine::InsightEngine() {
  // Register default rules
  addRule(std::make_unique<rules::SpillDetectedRule>());
  addRule(std::make_unique<rules::OutputBackpressureRule>());
  addRule(std::make_unique<rules::ScanLowSelectivityRule>());
  addRule(std::make_unique<rules::ScanIoBoundRule>());
  addRule(std::make_unique<rules::QueryStalledRule>());
}

std::string InsightEngine::generateKey(const InsightEvent& event) const {
  std::string key = event.kind;
  if (event.scope.planNodeId) {
    key += ":" + *event.scope.planNodeId;
  }
  return key;
}

std::vector<InsightEvent> InsightEngine::evaluate(
    const TaskSampleCollector& collector,
    const InsightOptions& options) {
  std::vector<InsightEvent> results;
  std::unordered_set<std::string> evaluatedKeys;

  for (const auto& rule : rules_) {
    auto optEvent = rule->evaluate(collector, options);
    if (optEvent) {
      InsightEvent event = *optEvent;
      std::string key = generateKey(event);
      evaluatedKeys.insert(key);

      auto it = openInsights_.find(key);
      if (it == openInsights_.end()) {
        // New insight
        event.state = InsightState::kNew;
        openInsights_[key] = event;
        results.push_back(event);
      } else {
        // Updated insight - we don't always emit an update event to avoid spam,
        // but we could depending on logic. For now let's just update the
        // tracked state. For simplicity, we won't emit "updated" events unless
        // evidence changes significantly.
        // TODO: Update logic
      }
    }
  }

  // Check for resolved insights
  for (auto it = openInsights_.begin(); it != openInsights_.end();) {
    if (evaluatedKeys.find(it->first) == evaluatedKeys.end()) {
      // Rule didn't trigger, but we had it open. Mark resolved.
      InsightEvent resolvedEvent = it->second;
      resolvedEvent.state = InsightState::kResolved;
      results.push_back(resolvedEvent);
      it = openInsights_.erase(it);
    } else {
      ++it;
    }
  }

  return results;
}

std::vector<std::string> InsightEngine::getOpenInsightKinds() const {
  std::unordered_set<std::string> kinds;
  for (const auto& [key, event] : openInsights_) {
    kinds.insert(event.kind);
  }
  return std::vector<std::string>(kinds.begin(), kinds.end());
}

} // namespace bytedance::bolt::exec::insights
