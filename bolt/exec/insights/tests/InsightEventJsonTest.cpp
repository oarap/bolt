#include "bolt/exec/insights/InsightEventJson.h"
#include <folly/json.h>
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;

TEST(InsightEventJsonTest, SerializeScope) {
  InsightScope scope;
  scope.planNodeId = "node_1";
  scope.operatorType = "TableScan";

  auto json = toJson(scope);
  EXPECT_TRUE(json.isObject());
  EXPECT_EQ(json["plan_node_id"].asString(), "node_1");
  EXPECT_EQ(json["operator_type"].asString(), "TableScan");
  EXPECT_TRUE(json.find("pipeline_id") == json.items().end());
}

TEST(InsightEventJsonTest, SerializeEvent) {
  InsightEvent event;
  event.schemaVersion = "1.0";
  event.sequenceId = 42;
  event.eventTimeMs = 123456789;
  event.queryId = "q1";
  event.taskId = "t1";
  event.kind = "spill_detected";
  event.phase = InsightPhase::kRuntime;
  event.state = InsightState::kNew;
  event.severity = InsightSeverity::kHigh;
  event.confidence = "high";
  event.scope.planNodeId = "node_2";
  event.evidence = folly::dynamic::object("spilled_bytes", 1024);
  event.recommendedActions = {"continue", "cancel_query"};
  event.humanSummary = "Spill detected on node_2";

  auto json = toJson(event);

  EXPECT_EQ(json["schema_version"].asString(), "1.0");
  EXPECT_EQ(json["sequence_id"].asInt(), 42);
  EXPECT_EQ(json["event_time_ms"].asInt(), 123456789);
  EXPECT_EQ(json["query_id"].asString(), "q1");
  EXPECT_EQ(json["task_id"].asString(), "t1");
  EXPECT_EQ(json["kind"].asString(), "spill_detected");
  EXPECT_EQ(json["phase"].asString(), "runtime");
  EXPECT_EQ(json["state"].asString(), "new");
  EXPECT_EQ(json["severity"].asString(), "high");
  EXPECT_EQ(json["confidence"].asString(), "high");
  EXPECT_EQ(json["scope"]["plan_node_id"].asString(), "node_2");
  EXPECT_EQ(json["evidence"]["spilled_bytes"].asInt(), 1024);
  EXPECT_EQ(
      json["recommended_actions"].size(),
      2); // Wait, field name is "recommended_actions"
  EXPECT_EQ(json["recommended_actions"][0].asString(), "continue");
  EXPECT_EQ(json["recommended_actions"][1].asString(), "cancel_query");
  EXPECT_EQ(json["human_summary"].asString(), "Spill detected on node_2");
}

TEST(InsightEventJsonTest, SerializeSnapshot) {
  QuerySnapshot snapshot;
  snapshot.queryId = "q2";
  snapshot.taskId = "t2";
  snapshot.taskState = "RUNNING";
  snapshot.startTimeMs = 1000;
  snapshot.elapsedMs = 500;
  snapshot.outputBufferUtilization = 0.85;
  snapshot.numTotalDrivers = 10;
  snapshot.numRunningDrivers = 8;
  snapshot.numBlockedDrivers = 2;
  snapshot.spilledBytes = 2048;
  snapshot.rawInputRows = 10000;
  snapshot.outputRows = 5000;
  snapshot.openInsightCount = 1;
  snapshot.openInsightKinds = {"scan_low_selectivity"};

  auto json = toJson(snapshot);

  EXPECT_EQ(json["query_id"].asString(), "q2");
  EXPECT_EQ(json["task_id"].asString(), "t2");
  EXPECT_EQ(json["task_state"].asString(), "RUNNING");
  EXPECT_EQ(json["start_time_ms"].asInt(), 1000);
  EXPECT_EQ(json["elapsed_ms"].asInt(), 500);
  EXPECT_DOUBLE_EQ(json["output_buffer_utilization"].asDouble(), 0.85);
  EXPECT_EQ(json["num_total_drivers"].asInt(), 10);
  EXPECT_EQ(json["num_running_drivers"].asInt(), 8);
  EXPECT_EQ(json["num_blocked_drivers"].asInt(), 2);
  EXPECT_EQ(json["spilled_bytes"].asInt(), 2048);
  EXPECT_EQ(json["raw_input_rows"].asInt(), 10000);
  EXPECT_EQ(json["output_rows"].asInt(), 5000);
  EXPECT_EQ(json["open_insight_count"].asInt(), 1);
  EXPECT_EQ(json["open_insight_kinds"][0].asString(), "scan_low_selectivity");
}
