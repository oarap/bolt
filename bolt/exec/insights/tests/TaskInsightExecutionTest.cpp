// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include "bolt/exec/insights/TaskInsightSession.h"
#include "bolt/exec/tests/utils/HiveConnectorTestBase.h"
#include "bolt/exec/tests/utils/PlanBuilder.h"
#include "bolt/exec/tests/utils/Cursor.h"
#include "bolt/connectors/hive/HiveConnector.h"
#include "bolt/connectors/hive/HiveDataSink.h"
#include "bolt/exec/Task.h"
#include "bolt/common/base/Exceptions.h"

using namespace bytedance::bolt;
using namespace bytedance::bolt::exec;
using namespace bytedance::bolt::exec::test;
using namespace bytedance::bolt::exec::insights;

namespace {

class TaskInsightExecutionTest : public HiveConnectorTestBase {
 protected:
  void SetUp() override {
    HiveConnectorTestBase::SetUp();
  }
};

TEST_F(TaskInsightExecutionTest, LiveScanExecution) {
  // Create test data
  auto data = makeRowVector({
      makeFlatVector<int64_t>(100, [](vector_size_t row) { return row; }),
      makeFlatVector<int64_t>(100, [](vector_size_t row) { return row * 2; })
  });
  
  auto rowType = asRowType(data->type());
  auto filePath = TempFilePath::create();
  writeToFile(filePath->path, std::vector<RowVectorPtr>{data});
  
  // Create a plan with a table scan that reads this file
  auto planNodeIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();
  core::PlanNodeId scanId;
  auto plan = PlanBuilder(planNodeIdGenerator)
                  .tableScan(rowType)
                  .capturePlanNodeId(scanId)
                  .planNode();

  // Setup cursor and task
  CursorParameters params;
  params.planNode = plan;
  
  // Use a mock connector split since we just wrote the file
  auto split = makeHiveConnectorSplit(filePath->path);
  
  auto taskCursor = TaskCursor::create(params);
  auto task = taskCursor->task();
  
  // Important part: attach the insight session to the live task
  SessionMetadata metadata;
  metadata.queryId = "test_query_123";
  InsightOptions options;
  // Make poll interval small for test
  options.pollIntervalMs = 10;
  options.minSamplesForRules = 1;
  options.minRowsForSelectivity = 10; // set strictly smaller than 100
  
  auto session = TaskInsightSession::attach(task, options, metadata);
  
  // Add the split and start execution
  task->addSplit(scanId, exec::Split(std::move(split)));
  task->noMoreSplits(scanId);
  
  // Poll a few times while running
  std::vector<InsightEvent> collectedEvents;
  
  while (taskCursor->moveNext()) {
    auto batch = taskCursor->current();

    // Let the task run and populate stats before we poll
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Poll the session
    auto events = session->poll();
    for (const auto& e : events) {
      collectedEvents.push_back(e);
    }
  }
  
  // After task finishes, poll one last time to resolve everything
  auto finalEvents = session->poll();
  for (const auto& e : finalEvents) {
    collectedEvents.push_back(e);
  }
  
  session->close(); // ensure resolution of samples
  auto snapshot = session->snapshot();
  EXPECT_EQ(snapshot.taskState, "Finished");
  
  // Verify we saw a scan_low_selectivity insight from the real plan node
  bool foundLowSelectivity = false;
  for (const auto& event : collectedEvents) {
    // Print what we got for debugging
    std::cout << "Got event: " << event.kind << " state: " << (int)event.state << std::endl;
    if (event.kind == "scan_low_selectivity" && event.state == InsightState::kNew) {
      foundLowSelectivity = true;
      EXPECT_EQ(event.scope.planNodeId, scanId);
      EXPECT_EQ(event.scope.operatorType, "TableScan");
      EXPECT_EQ(event.queryId, "test_query_123");
      EXPECT_EQ(event.severity, InsightSeverity::kMedium);
    }
  }
  
  // We expect scan_low_selectivity to be found because it's a pass-through scan
  // without a filter, so selectivity is 1.0 (100%), which is bad selectivity!
  EXPECT_TRUE(foundLowSelectivity);
}

} // namespace
