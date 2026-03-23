#include "bolt/exec/insights/rules/ScanLowSelectivityRule.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;
using namespace bytedance::bolt::exec::insights::rules;

TEST(ScanLowSelectivityRuleTest, GoodSelectivity) {
  ScanLowSelectivityRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  
  bytedance::bolt::exec::PipelineStats pipeStats(true, false, false);
  bytedance::bolt::exec::OperatorStats opStats(1, 1, "node_1", "TableScan", false, {});
  opStats.rawInputPositions = 20000;
  opStats.outputPositions = 1000; // 5% pass through
  pipeStats.operatorStats.push_back(opStats);
  sample.taskStats.pipelineStats.push_back(pipeStats);

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  EXPECT_FALSE(result.has_value());
}

TEST(ScanLowSelectivityRuleTest, BadSelectivity) {
  ScanLowSelectivityRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  
  bytedance::bolt::exec::PipelineStats pipeStats(true, false, false);
  bytedance::bolt::exec::OperatorStats opStats(1, 1, "node_1", "TableScan", false, {});
  opStats.rawInputPositions = 20000;
  opStats.outputPositions = 19500; // 97.5% pass through
  pipeStats.operatorStats.push_back(opStats);
  sample.taskStats.pipelineStats.push_back(pipeStats);

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->kind, "scan_low_selectivity");
  EXPECT_DOUBLE_EQ(result->evidence["pass_through_ratio"].asDouble(), 19500.0 / 20000.0);
}
