// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#include "bolt/tool/insights/InsightDemoRunner.h"
#include <chrono>
#include <iostream>
#include <thread>
#include "bolt/exec/Task.h"
#include "bolt/exec/tests/utils/PlanBuilder.h"
#include "bolt/exec/tests/utils/OperatorTestBase.h"
#include "bolt/connectors/hive/HiveConnector.h"
#include "bolt/exec/tests/utils/TempFilePath.h"
#include "bolt/dwio/dwrf/writer/DwrfWriter.h"

namespace bytedance::bolt::tool::insights {

// Helper to construct a real Bolt plan
static std::shared_ptr<exec::TaskCursor> createLiveDemoTask(
    const std::shared_ptr<core::PlanNodeIdGenerator>& planNodeIdGenerator) {
  
  // Create synthetic data
  auto rowType = ROW({"c0", "c1"}, {BIGINT(), BIGINT()});
  auto pool = memory::MemoryManager::getInstance()->addLeafPool("DemoPool");
  auto vectors = exec::test::makeBatches(rowType, 5, 100, pool.get());
  auto tempFile = exec::test::TempFilePath::create();
  exec::test::writeToFile(tempFile->path, vectors);

  // Build a scan plan
  core::PlanNodeId scanId;
  auto plan = exec::test::PlanBuilder(planNodeIdGenerator)
                  .tableScan(rowType)
                  .capturePlanNodeId(scanId)
                  .planNode();

  // Create cursor
  exec::CursorParameters params;
  params.planNode = plan;
  auto taskCursor = exec::TaskCursor::create(params);
  
  // Add split
  auto split = exec::test::makeHiveConnectorSplit(tempFile->path);
  taskCursor->task()->addSplit(scanId, exec::Split(split));
  taskCursor->task()->noMoreSplits(scanId);
  
  return taskCursor;
}

InsightDemoRunner::InsightDemoRunner() {}

void InsightDemoRunner::runScenario(const std::string& scenarioName) {
  std::cout << "Running scenario: " << scenarioName << std::endl;
  
  auto planNodeIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();
  auto taskCursor = createLiveDemoTask(planNodeIdGenerator);
  
  exec::insights::SessionMetadata metadata;
  metadata.queryId = "demo_query_" + scenarioName;
  
  exec::insights::InsightOptions options;
  // Use small thresholds to make demo output reliable
  options.minRowsForSelectivity = 10;
  options.lowSelectivityThreshold = 0.5;
  
  auto session = exec::insights::TaskInsightSession::attach(taskCursor->task(), metadata, options);
  
  // Start execution thread
  std::thread executionThread([taskCursor]() {
    while (taskCursor->hasNext()) {
      auto batch = taskCursor->next();
      // Add a tiny sleep to make it visible
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  });
  
  runWithPolicy(session);
  
  executionThread.join();
}

void InsightDemoRunner::snapshot() {
  std::cout << "Snapshot command not yet connected to a live task" << std::endl;
}

void InsightDemoRunner::stream() {
  std::cout << "Stream command not yet connected to a live task" << std::endl;
}

void InsightDemoRunner::cancel() {
  std::cout << "Cancel command not yet connected to a live task" << std::endl;
}

void InsightDemoRunner::runWithPolicy(
    const std::shared_ptr<exec::insights::TaskInsightSession>& session) {
  std::cout << "Starting task execution with policy agent monitoring..."
            << std::endl;

  bool running = true;
  while (running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    auto events = session->poll();

    // Print new events
    for (const auto& event : events) {
      std::cout << "\n[INSIGHT] " << event.kind
                << " (Severity: " << static_cast<int>(event.severity) << ")"
                << std::endl;
    }

    // Agent evaluates
    auto decision = agent_.evaluate(events);

    switch (decision.action) {
      case PolicyAgent::Action::kCancel:
        std::cout << "\n[AGENT DECISION] Canceling query: " << decision.reason
                  << std::endl;
        session->cancel();
        running = false;
        break;
      case PolicyAgent::Action::kRerun:
        std::cout << "\n[AGENT DECISION] Suggesting rerun: " << decision.reason
                  << std::endl;
        // In a real system, we might cancel and automatically rewrite/rerun
        // here
        break;
      case PolicyAgent::Action::kContinue:
        // Everything is fine
        break;
    }

    // Check if task finished on its own
    auto snap = session->snapshot();
    if (snap.taskState == "FINISHED" || snap.taskState == "CANCELED" ||
        snap.taskState == "FAILED") {
      std::cout << "Task terminal state reached: " << snap.taskState
                << std::endl;
      running = false;
    }
  }
}

} // namespace bytedance::bolt::tool::insights
