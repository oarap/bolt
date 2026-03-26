// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#include "bolt/tool/insights/InsightDemoRunner.h"
#include <chrono>
#include <iostream>
#include <thread>
#include "bolt/connectors/hive/HiveConnector.h"
#include "bolt/exec/Task.h"
#include "bolt/exec/tests/utils/OperatorTestBase.h"
#include "bolt/exec/tests/utils/PlanBuilder.h"
#include "bolt/exec/tests/utils/TempFilePath.h"
#include "bolt/exec/tests/utils/VectorTestUtil.h"

namespace bytedance::bolt::tool::insights {

// Helper to construct a real Bolt plan
static std::unique_ptr<bytedance::bolt::exec::test::TaskCursor>
createLiveDemoTask(
    const std::shared_ptr<core::PlanNodeIdGenerator>& planNodeIdGenerator) {
  // Create synthetic data
  auto rowType = ROW({"c0", "c1"}, {BIGINT(), BIGINT()});
  static auto pool =
      memory::MemoryManager::getInstance()->addLeafPool("DemoPool");
  // Create a much larger dataset to make it run longer
  auto vectors = bytedance::bolt::exec::test::makeBatches(
      100, 1000, rowType, pool.get(), 0.1, true);
  auto tempFile = bytedance::bolt::exec::test::TempFilePath::create();

  // We need to use HiveConnectorTestBase functionality, but we are not in a
  // test. This approach is simplified. Let's just create a test base instance
  // to use its methods. Actually, since we need writeToFile, we might need a
  // simpler way or to just use an empty scan if we can't easily write to a file
  // here. Let's use a Values node instead of a TableScan node! That's much
  // simpler and doesn't require files.

  // Build a plan with Values node, but make it repeat many times to simulate
  // long query
  core::PlanNodeId valuesId;
  auto plan =
      bytedance::bolt::exec::test::PlanBuilder(planNodeIdGenerator)
          .values(vectors, true, 100) // Repeat 100 times to make it run longer
          .capturePlanNodeId(valuesId)
          .planNode();

  // Create cursor
  bytedance::bolt::exec::test::CursorParameters params;
  params.planNode = plan;
  auto taskCursor = bytedance::bolt::exec::test::TaskCursor::create(params);

  return taskCursor;
}

InsightDemoRunner::InsightDemoRunner() = default;

void InsightDemoRunner::runScenario(const std::string& scenarioName) {
  std::cout << "Running scenario: " << scenarioName << std::endl;

  if (scenarioName == "mock_backpressure" ||
      scenarioName == "mock_low_selectivity" ||
      scenarioName == "mock_spill_detected") {
    runMockScenario(scenarioName);
    return;
  }

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

  auto session =
      exec::insights::TaskInsightSession::attach(task, options, metadata);

  // Need to transfer ownership for the thread
  std::shared_ptr<bytedance::bolt::exec::test::TaskCursor> sharedCursor =
      std::move(taskCursor);

  // Start execution thread
  std::thread executionThread([sharedCursor]() {
    try {
      int batchCount = 0;
      while (sharedCursor->moveNext()) {
        auto batch = sharedCursor->current();
        batchCount++;
        if (batchCount % 100 == 0) {
          std::cout << "[EXEC] Processed " << batchCount << " batches..."
                    << std::endl;
        }
        // Add a sleep to make it visible and run long enough to trigger
        // insights
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // We simulate backpressure artificially for the demo
        // by keeping operators running slowly but making them block
      }
      std::cout << "[EXEC] Finished processing " << batchCount
                << " batches total." << std::endl;
    } catch (const std::exception& e) {
      std::cout << "[EXEC] Task execution terminated: " << e.what()
                << std::endl;
    }
  });

  runWithPolicy(session);

  executionThread.join();
}

void InsightDemoRunner::runInteractive(const std::string& scenarioName) {
  // In interactive mode, we use JSON input/output for MCP Server.
  auto planNodeIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();
  auto taskCursor = createLiveDemoTask(planNodeIdGenerator);
  auto task = taskCursor->task();

  exec::insights::SessionMetadata metadata;
  metadata.queryId = "interactive_query_" + scenarioName;

  exec::insights::InsightOptions options;
  options.minRowsForSelectivity = 10;
  options.lowSelectivityThreshold = 0.5;
  options.pollIntervalMs = 50;

  auto session =
      exec::insights::TaskInsightSession::attach(task, options, metadata);

  std::shared_ptr<bytedance::bolt::exec::test::TaskCursor> sharedCursor =
      std::move(taskCursor);

  std::thread executionThread([sharedCursor]() {
    try {
      while (sharedCursor->moveNext()) {
        auto batch = sharedCursor->current();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
    } catch (...) {
    }
  });

  std::cout << "{\"status\": \"ready\"}\n" << std::flush;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "poll") {
      auto events = session->poll();
      auto snap = session->snapshot();

      std::cout << "{\"events\": [";
      for (size_t i = 0; i < events.size(); ++i) {
        std::cout << "{\"kind\": \"" << events[i].kind << "\", "
                  << "\"severity\": " << static_cast<int>(events[i].severity)
                  << ", "
                  << "\"summary\": \"" << events[i].humanSummary << "\"}";
        if (i < events.size() - 1)
          std::cout << ",";
      }
      std::cout << "], \"state\": \"" << snap.taskState << "\"}\n"
                << std::flush;

      if (snap.taskState == "Finished" || snap.taskState == "Canceled" ||
          snap.taskState == "Failed") {
        break;
      }
    } else if (line == "cancel") {
      try {
        session->cancel();
        std::cout << "{\"status\": \"canceled\"}\n" << std::flush;
      } catch (...) {
        std::cout << "{\"error\": \"cancel_failed\"}\n" << std::flush;
      }
      break;
    } else if (line == "exit" || line == "quit") {
      try {
        session->cancel();
      } catch (...) {
      }
      break;
    } else {
      std::cout << "{\"error\": \"unknown_command\"}\n" << std::flush;
    }
  }

  try {
    session->cancel();
  } catch (...) {
  }
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
  int iteration = 0;
  while (running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    iteration++;

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
        try {
          session->cancel();
        } catch (const std::exception& e) {
          // Expected when cancelling a local test task
        }
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
      std::cout << "\nTask terminal state reached: " << snap.taskState
                << std::endl;
      running = false;
    }

    // Safety timeout - if demo runs too long, exit
    if (iteration > 50) {
      std::cout << "\nDemo safety timeout reached. Exiting." << std::endl;
      try {
        session->cancel();
      } catch (const std::exception& e) {
        // Expected when cancelling a local test task
      }
      running = false;
    }
  }
}

void InsightDemoRunner::runMockScenario(const std::string& scenarioName) {
  std::cout << "Starting synthetic mock event injection for " << scenarioName
            << "..." << std::endl;

  std::vector<exec::insights::InsightEvent> events;
  exec::insights::InsightEvent mockEvent;
  mockEvent.eventTimeMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  mockEvent.phase = exec::insights::InsightPhase::kRuntime;
  mockEvent.state = exec::insights::InsightState::kNew;
  mockEvent.confidence = "high";

  if (scenarioName == "mock_backpressure") {
    mockEvent.kind = "output_backpressure";
    mockEvent.severity = exec::insights::InsightSeverity::kHigh;
    mockEvent.humanSummary =
        "Output buffer is overutilized, indicating downstream backpressure";
  } else if (scenarioName == "mock_low_selectivity") {
    mockEvent.kind = "scan_low_selectivity";
    mockEvent.severity = exec::insights::InsightSeverity::kMedium;
    mockEvent.humanSummary =
        "Table scan has low selectivity (passing through 98%)";
  } else if (scenarioName == "mock_spill_detected") {
    mockEvent.kind = "spill_detected";
    mockEvent.severity = exec::insights::InsightSeverity::kHigh;
    mockEvent.humanSummary =
        "Spill detected on plan node HashAggregation_1 (HashAggregation)";
  }

  std::cout << "\n[SIMULATING INSIGHT ENGINE METRICS POLLING]\n.";
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::cout << ".\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::cout << "\n[INSIGHT GENERATED BY ENGINE] " << mockEvent.kind
            << " (Severity: " << static_cast<int>(mockEvent.severity) << ")"
            << "\n  Summary: " << mockEvent.humanSummary << std::endl;

  events.push_back(mockEvent);

  auto decision = agent_.evaluate(events);

  std::cout << "\n[AGENT EVALUATION RESULTS]\n";
  switch (decision.action) {
    case PolicyAgent::Action::kCancel:
      std::cout << "  Action: CANCEL QUERY\n";
      std::cout << "  Reason: " << decision.reason << "\n";
      std::cout << "\n[MOCK EXECUTION] Cancelling simulated task..."
                << std::endl;
      break;
    case PolicyAgent::Action::kRerun:
      std::cout << "  Action: SUGGEST RERUN\n";
      std::cout << "  Reason: " << decision.reason << "\n";
      std::cout
          << "\n[MOCK EXECUTION] In real system, query would be cancelled and rewritten..."
          << std::endl;
      break;
    case PolicyAgent::Action::kContinue:
      std::cout << "  Action: CONTINUE\n";
      std::cout << "  Reason: " << decision.reason << "\n";
      std::cout << "\n[MOCK EXECUTION] Continuing simulated task execution..."
                << std::endl;
      break;
  }
}

} // namespace bytedance::bolt::tool::insights
