#include "bolt/exec/insights/TaskInsightSession.h"
#include <folly/json.h>
#include <fstream>
#include "bolt/common/time/Timer.h"
#include "bolt/exec/TaskStructs.h"
#include "bolt/exec/insights/InsightEngine.h"
#include "bolt/exec/insights/InsightEventJson.h"

namespace bytedance::bolt::exec::insights {

TaskInsightSession::TaskInsightSession(
    std::shared_ptr<Task> task,
    InsightOptions options,
    SessionMetadata metadata)
    : task_(std::move(task)),
      options_(std::move(options)),
      metadata_(std::move(metadata)),
      sampleCollector_(options_.historyWindowSize),
      engine_(std::make_unique<InsightEngine>()) {}

TaskInsightSession::~TaskInsightSession() {
  close();
}

std::shared_ptr<TaskInsightSession> TaskInsightSession::attach(
    const std::shared_ptr<Task>& task,
    InsightOptions options,
    SessionMetadata metadata) {
  if (!task) {
    throw std::invalid_argument("Task cannot be null");
  }
  return std::shared_ptr<TaskInsightSession>(
      new TaskInsightSession(task, std::move(options), std::move(metadata)));
}

void TaskInsightSession::doPoll() {
  if (closed_) {
    return;
  }

  TaskSample sample;
  sample.timestampMs = bytedance::bolt::getCurrentTimeMs();
  sample.taskStats = task_->taskStats();
  sampleCollector_.addSample(std::move(sample));

  auto state = task_->state();
  if (state == TaskState::kFinished || state == TaskState::kCanceled ||
      state == TaskState::kAborted || state == TaskState::kFailed) {
    close();
  }
}

std::vector<InsightEvent> TaskInsightSession::poll() {
  doPoll();

  if (!engine_)
    return {};

  auto events = engine_->evaluate(sampleCollector_, options_);

  // Set sequence ids and metadata
  for (auto& event : events) {
    event.sequenceId = ++nextSequenceId_;
    event.queryId = metadata_.queryId;
    event.taskId = metadata_.taskId ? metadata_.taskId : task_->taskId();
  }

  // Write to a well-known file for the MCP server to pick up
  if (!events.empty()) {
    std::ofstream out("/tmp/bolt_insights.json", std::ios::app);
    for (const auto& event : events) {
      out << folly::toJson(toJson(event)) << std::endl;
    }
    out.close();
  }

  return events;
}

QuerySnapshot TaskInsightSession::snapshot() const {
  QuerySnapshot snapshot;
  snapshot.queryId = metadata_.queryId;
  snapshot.taskId = metadata_.taskId ? metadata_.taskId : task_->taskId();

  auto latest = sampleCollector_.getLatestSample();
  if (latest) {
    const auto& stats = latest->taskStats;
    snapshot.taskState = taskStateString(task_->state());
    snapshot.startTimeMs = stats.executionStartTimeMs;
    snapshot.elapsedMs = snapshot.startTimeMs > 0
        ? (latest->timestampMs - snapshot.startTimeMs)
        : 0;

    snapshot.outputBufferUtilization = stats.outputBufferUtilization;
    snapshot.numTotalDrivers = stats.numTotalDrivers;
    snapshot.numRunningDrivers = stats.numRunningDrivers;
    int blockedSum = 0;
    for (const auto& [reason, count] : stats.numBlockedDrivers) {
      blockedSum += count;
    }
    snapshot.numBlockedDrivers = blockedSum;

    uint64_t spilledBytes = 0;
    uint64_t rawInputRows = 0;
    uint64_t outputRows = 0;
    for (const auto& pipeline : stats.pipelineStats) {
      for (const auto& op : pipeline.operatorStats) {
        spilledBytes += op.spilledBytes;
        rawInputRows += op.rawInputPositions;
        if (pipeline.outputPipeline) {
          // Approximation: sum of output positions from output pipeline ops
          outputRows += op.outputPositions;
        }
      }
    }
    snapshot.spilledBytes = spilledBytes;
    snapshot.rawInputRows = rawInputRows;
    snapshot.outputRows = outputRows;

    if (engine_) {
      snapshot.openInsightCount = engine_->getOpenInsightCount();
      snapshot.openInsightKinds = engine_->getOpenInsightKinds();
    }
  }

  return snapshot;
}

bytedance::bolt::ContinueFuture TaskInsightSession::cancel() {
  return task_->requestCancel();
}

void TaskInsightSession::close() {
  bool expected = false;
  if (closed_.compare_exchange_strong(expected, true)) {
    doClose();
  }
}

void TaskInsightSession::doClose() {
  // Cleanup if needed
}

} // namespace bytedance::bolt::exec::insights
