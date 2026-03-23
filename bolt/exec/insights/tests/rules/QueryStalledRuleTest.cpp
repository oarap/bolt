#include "bolt/exec/insights/rules/QueryStalledRule.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;
using namespace bytedance::bolt::exec::insights::rules;

TEST(QueryStalledRuleTest, RunningQuery) {
  QueryStalledRule rule;
  TaskSampleCollector collector(3);
  InsightOptions options;

  for (int i = 0; i < 3; i++) {
    TaskSample sample;
    sample.timestampMs = 100 * (i + 1);
    sample.taskStats.numTotalDrivers = 10;
    sample.taskStats.numRunningDrivers = 2; // Always some running
    sample.taskStats.numCompletedDrivers = 0;
    collector.addSample(sample);
  }

  auto result = rule.evaluate(collector, options);
  EXPECT_FALSE(result.has_value());
}

TEST(QueryStalledRuleTest, StalledQuery) {
  QueryStalledRule rule;
  TaskSampleCollector collector(3);
  InsightOptions options;

  for (int i = 0; i < 3; i++) {
    TaskSample sample;
    sample.timestampMs = 100 * (i + 1);
    sample.taskStats.numTotalDrivers = 10;
    sample.taskStats.numRunningDrivers = 0; // None running
    sample.taskStats.numCompletedDrivers = 5;
    sample.taskStats.numBlockedDrivers[bytedance::bolt::exec::BlockingReason::kWaitForProducer] = 5;
    collector.addSample(sample);
  }

  auto result = rule.evaluate(collector, options);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->kind, "query_stalled");
  EXPECT_EQ(result->evidence["stalled_samples"].asInt(), 3);
}
