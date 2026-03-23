#include "bolt/exec/insights/rules/ScanIoBoundRule.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;
using namespace bytedance::bolt::exec::insights::rules;

TEST(ScanIoBoundRuleTest, NormalScan) {
  ScanIoBoundRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  
  bytedance::bolt::exec::PipelineStats pipeStats(true, false, false);
  bytedance::bolt::exec::OperatorStats opStats(1, 1, "node_1", "TableScan", false, {});
  
  bytedance::bolt::RuntimeMetric ioWait;
  ioWait.sum = 100000000; // 0.1s
  bytedance::bolt::RuntimeMetric totalTime;
  totalTime.sum = 2000000000; // 2s
  
  opStats.runtimeStats["ioWaitWallNanos"] = ioWait;
  opStats.runtimeStats["totalScanTime"] = totalTime;
  
  pipeStats.operatorStats.push_back(opStats);
  sample.taskStats.pipelineStats.push_back(pipeStats);

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  EXPECT_FALSE(result.has_value());
}

TEST(ScanIoBoundRuleTest, IoBoundScan) {
  ScanIoBoundRule rule;
  TaskSampleCollector collector(5);
  InsightOptions options;

  TaskSample sample;
  sample.timestampMs = 100;
  
  bytedance::bolt::exec::PipelineStats pipeStats(true, false, false);
  bytedance::bolt::exec::OperatorStats opStats(1, 1, "node_1", "TableScan", false, {});
  
  bytedance::bolt::RuntimeMetric ioWait;
  ioWait.sum = 1800000000; // 1.8s
  bytedance::bolt::RuntimeMetric totalTime;
  totalTime.sum = 2000000000; // 2s
  
  opStats.runtimeStats["ioWaitWallNanos"] = ioWait;
  opStats.runtimeStats["totalScanTime"] = totalTime;
  
  pipeStats.operatorStats.push_back(opStats);
  sample.taskStats.pipelineStats.push_back(pipeStats);

  collector.addSample(sample);
  auto result = rule.evaluate(collector, options);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->kind, "scan_io_bound");
  EXPECT_DOUBLE_EQ(result->evidence["io_wait_ratio"].asDouble(), 0.9);
}
