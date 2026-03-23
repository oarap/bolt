#include "bolt/exec/insights/rules/SpillDetectedRule.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;
using namespace bytedance::bolt::exec::insights::rules;

TEST(SpillDetectedRuleTest, NoSpill) {
  SpillDetectedRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  
  bytedance::bolt::exec::PipelineStats pipeStats(false, false, false);
  bytedance::bolt::exec::OperatorStats opStats(1, 1, "node_1", "HashBuild", false, {});
  opStats.spilledBytes = 0; // No spill
  pipeStats.operatorStats.push_back(opStats);
  sample.taskStats.pipelineStats.push_back(pipeStats);

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  EXPECT_FALSE(result.has_value());
}

TEST(SpillDetectedRuleTest, SpillDetectedAndDeduplicated) {
  SpillDetectedRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample1;
  sample1.timestampMs = 100;
  
  bytedance::bolt::exec::PipelineStats pipeStats(false, false, false);
  bytedance::bolt::exec::OperatorStats opStats(1, 1, "node_1", "HashBuild", false, {});
  opStats.spilledBytes = 1024; // Spilled
  pipeStats.operatorStats.push_back(opStats);
  sample1.taskStats.pipelineStats.push_back(pipeStats);

  collector.addSample(sample1);
  auto result1 = rule.evaluate(collector, options);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(result1->kind, "spill_detected");
  EXPECT_EQ(result1->scope.planNodeId.value(), "node_1");

  // Second sample, still spilling
  TaskSample sample2 = sample1;
  sample2.timestampMs = 200;
  sample2.taskStats.pipelineStats[0].operatorStats[0].spilledBytes = 2048;

  collector.addSample(sample2);
  auto result2 = rule.evaluate(collector, options);
  // Should be deduped by node
  EXPECT_FALSE(result2.has_value());
}
