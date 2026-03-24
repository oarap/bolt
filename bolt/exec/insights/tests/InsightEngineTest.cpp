#include "bolt/exec/insights/InsightEngine.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;

class MockRule : public InsightRule {
 public:
  explicit MockRule(const std::string& name) : name_(name) {}

  std::string name() const override {
    return name_;
  }

  std::optional<InsightEvent> evaluate(
      const TaskSampleCollector& collector,
      const InsightOptions& options) override {
    if (shouldTrigger) {
      InsightEvent event;
      event.kind = name_;
      event.scope.planNodeId = "node_1";
      return event;
    }
    return std::nullopt;
  }

  bool shouldTrigger = false;

 private:
  std::string name_;
};

TEST(InsightEngineTest, StateTransitions) {
  InsightEngine engine;
  TaskSampleCollector collector(5);
  InsightOptions options;

  // Since we add default rules in the constructor, we can clear them to test
  // our mock Or just test the default rules. Let's test with default rules to
  // ensure it works.

  // Test with OutputBackpressureRule
  TaskSample sample;
  sample.timestampMs = 100;
  sample.taskStats.outputBufferUtilization = 0.5; // Healthy
  collector.addSample(sample);

  auto events = engine.evaluate(collector, options);
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(engine.getOpenInsightCount(), 0);

  // Trigger insight
  TaskSample sample2;
  sample2.timestampMs = 200;
  sample2.taskStats.outputBufferUtilization = 0.95; // Unhealthy
  collector.addSample(sample2);

  events = engine.evaluate(collector, options);
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].kind, "output_backpressure");
  EXPECT_EQ(events[0].state, InsightState::kNew);
  EXPECT_EQ(engine.getOpenInsightCount(), 1);

  // Still unhealthy - shouldn't emit new event (deduplication)
  TaskSample sample3;
  sample3.timestampMs = 300;
  sample3.taskStats.outputBufferUtilization = 0.95;
  collector.addSample(sample3);

  events = engine.evaluate(collector, options);
  EXPECT_TRUE(events.empty());
  EXPECT_EQ(engine.getOpenInsightCount(), 1);

  // Healthy again - should resolve
  TaskSample sample4;
  sample4.timestampMs = 400;
  sample4.taskStats.outputBufferUtilization = 0.5;
  collector.addSample(sample4);

  events = engine.evaluate(collector, options);
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].kind, "output_backpressure");
  EXPECT_EQ(events[0].state, InsightState::kResolved);
  EXPECT_EQ(engine.getOpenInsightCount(), 0);
}
