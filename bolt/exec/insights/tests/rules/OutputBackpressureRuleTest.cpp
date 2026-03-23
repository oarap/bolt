#include "bolt/exec/insights/rules/OutputBackpressureRule.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;
using namespace bytedance::bolt::exec::insights::rules;

TEST(OutputBackpressureRuleTest, HealthyBuffer) {
  OutputBackpressureRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  sample.taskStats.outputBufferUtilization = 0.5;
  sample.taskStats.outputBufferOverutilized = false;

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  EXPECT_FALSE(result.has_value());
}

TEST(OutputBackpressureRuleTest, OverutilizedBuffer) {
  OutputBackpressureRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  sample.taskStats.outputBufferUtilization = 0.95;
  sample.taskStats.outputBufferOverutilized = true;

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->kind, "output_backpressure");
  EXPECT_EQ(result->evidence["output_buffer_utilization"].asDouble(), 0.95);
}
