// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include "bolt/tool/insights/InsightDemoRunner.h"
#include "bolt/tool/insights/PolicyAgent.h"

namespace bytedance::bolt::tool::insights::test {

class InsightDemoRunnerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(InsightDemoRunnerTest, PolicyAgentDecisions) {
  PolicyAgent agent;
  std::vector<exec::insights::InsightEvent> events;

  // No events -> continue
  auto decision = agent.evaluate(events);
  EXPECT_EQ(decision.action, PolicyAgent::Action::kContinue);

  // Info severity -> continue
  exec::insights::InsightEvent infoEvent;
  infoEvent.kind = "scan_low_selectivity";
  infoEvent.severity = exec::insights::InsightSeverity::kInfo;
  infoEvent.state = exec::insights::InsightState::kNew;
  events.push_back(infoEvent);
  
  decision = agent.evaluate(events);
  EXPECT_EQ(decision.action, PolicyAgent::Action::kContinue);

  // High severity backpressure -> cancel
  exec::insights::InsightEvent highEvent;
  highEvent.kind = "output_backpressure";
  highEvent.severity = exec::insights::InsightSeverity::kHigh;
  highEvent.state = exec::insights::InsightState::kNew;
  events.push_back(highEvent);

  decision = agent.evaluate(events);
  EXPECT_EQ(decision.action, PolicyAgent::Action::kCancel);
}

TEST_F(InsightDemoRunnerTest, BasicRun) {
  // Empty test for now, will implement actual tests in Phase 2.4
  EXPECT_TRUE(true);
}

} // namespace bytedance::bolt::tool::insights::test
