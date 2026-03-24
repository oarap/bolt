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
#include "bolt/exec/tests/utils/VectorTestUtil.h"

namespace bytedance::bolt::tool::insights {

// Helper to construct a real Bolt plan
static std::unique_ptr<bytedance::bolt::exec::test::TaskCursor> createLiveDemoTask(
    const std::shared_ptr<core::PlanNodeIdGenerator>& planNodeIdGenerator) {
  
  // Create synthetic data
  auto rowType = ROW({"c0", "c1"}, {BIGINT(), BIGINT()});
  static auto pool = memory::MemoryManager::getInstance()->addLeafPool("DemoPool");
  auto vectors = bytedance::bolt::exec::test::makeBatches(5, 100, rowType, pool.get(), 0.1, true);
  auto tempFile = bytedance::bolt::exec::test::TempFilePath::create();
  
  // We need to use HiveConnectorTestBase functionality, but we are not in a test.
  // This approach is simplified. Let's just create a test base instance to use its methods.
  // Actually, since we need writeToFile, we might need a simpler way or to just use
  // an empty scan if we can't easily write to a file here.
  // Let's use a Values node instead of a TableScan node! That's much simpler and doesn't require files.
  
  // Build a plan with Values node
  core::PlanNodeId valuesId;
  auto plan = bytedance::bolt::exec::test::PlanBuilder(planNodeIdGenerator)
                  .values(vectors)
                  .capturePlanNodeId(valuesId)
                  // Add a slow projection to make the query run longer and simulate work
                  .project({"c0", "c1"}) 
                  .planNode();

  // Create cursor
  bytedance::bolt::exec::test::CursorParameters params;
  params.planNode = plan;
  auto taskCursor = bytedance::bolt::exec::test::TaskCursor::create(params);
  
  return taskCursor;
}

InsightDemoRunner::InsightDemoRunner() {}

void InsightDemoRunner::runScenario(const std::string& scenarioName) {
  std::cout << "Running scenario: " << scenarioName << std::endl;
  
  auto planNodeIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();
  auto taskCursor = createLiveDemoTask(planNodeIdGenerator);
  auto task = taskCursor->task();
  
  exec::insights::SessionMetadata metadata;
  metadata.queryId = "demo_query_" + scenarioName;
  
  exec::insights::InsightOptions options;
  // Use small thresholds to make demo output reliable
  options.minRowsForSelectivity = 10;
  options.lowSelectivityThreshold = 0.5;
  options.pollIntervalMs = 50; // faster polling for demo
  
  auto session = exec::insights::TaskInsightSession::attach(task, options, metadata);
  
  // Need to transfer ownership for the thread
  std::shared_ptr<bytedance::bolt::exec::test::TaskCursor> sharedCursor = std::move(taskCursor);
  
  // Start execution thread
  std::thread executionThread([sharedCursor]() {
    while (sharedCursor->moveNext()) {
      auto batch = sharedCursor->current();
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
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
        // Print dot to show it's monitoring
        std::cout << "." << std::flush;
        break;
    }

    // Check if task finished on its own
    auto snap = session->snapshot();
    if (snap.taskState == "Finished" || snap.taskState == "Canceled" ||
        snap.taskState == "Failed") {
      std::cout << "Task terminal state reached: " << snap.taskState
                << std::endl;
      running = false;
    }
  }
}

} // namespace bytedance::bolt::tool::insights
