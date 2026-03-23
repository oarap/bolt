#pragma once

#include <folly/dynamic.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bolt::exec::insights {

enum class InsightState { kNew, kUpdated, kResolved };

enum class InsightPhase { kPreflight, kRuntime, kTerminal };

enum class InsightSeverity { kInfo, kLow, kMedium, kHigh };

struct InsightScope {
  std::optional<std::string> planNodeId;
  std::optional<std::string> operatorType;
  std::optional<int32_t> pipelineId;
  std::optional<int32_t> driverGroup;
  std::optional<std::string> taskId;
};

struct InsightEvent {
  std::string schemaVersion{"1.0"};
  int64_t sequenceId{0};
  int64_t eventTimeMs{0};
  std::optional<std::string> queryId;
  std::optional<std::string> taskId;
  std::string kind;
  InsightPhase phase{InsightPhase::kRuntime};
  InsightState state{InsightState::kNew};
  InsightSeverity severity{InsightSeverity::kInfo};
  std::string confidence; // e.g., "high", "medium", "low"
  InsightScope scope;
  folly::dynamic evidence = folly::dynamic::object;
  std::vector<std::string> recommendedActions;
  std::string humanSummary;
};

struct SessionMetadata {
  std::optional<std::string> queryId;
  std::optional<std::string> taskId;
  std::optional<std::string> stageId;
  std::optional<std::string> fragmentId;
  std::optional<std::string> sqlHash;
};

struct QuerySnapshot {
  std::optional<std::string> queryId;
  std::optional<std::string> taskId;
  std::string taskState;
  int64_t startTimeMs{0};
  int64_t elapsedMs{0};
  double outputBufferUtilization{0.0};
  int32_t numTotalDrivers{0};
  int32_t numRunningDrivers{0};
  int32_t numBlockedDrivers{0};
  int64_t spilledBytes{0};
  int64_t rawInputRows{0};
  int64_t outputRows{0};
  int32_t openInsightCount{0};
  std::vector<std::string> openInsightKinds;
};

} // namespace bolt::exec::insights
