#include "bolt/exec/insights/TaskInsightSession.h"
#include "bolt/common/time/Timer.h"

namespace bytedance::bolt::exec::insights {

TaskInsightSession::TaskInsightSession(
    std::shared_ptr<Task> task,
    InsightOptions options,
    SessionMetadata metadata)
    : task_(std::move(task)),
      options_(std::move(options)),
      metadata_(std::move(metadata)),
      sampleCollector_(options_.historyWindowSize) {}

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
}

std::vector<InsightEvent> TaskInsightSession::poll() {
  doPoll();
  // We don't have the engine yet, so return empty events.
  return {};
}

QuerySnapshot TaskInsightSession::snapshot() const {
  QuerySnapshot snapshot;
  snapshot.queryId = metadata_.queryId;
  snapshot.taskId = metadata_.taskId ? metadata_.taskId : task_->taskId();

  auto latest = sampleCollector_.getLatestSample();
  if (latest) {
    const auto& stats = latest->taskStats;
    // Assuming task state can be converted to string, for now a placeholder
    snapshot.taskState = "RUNNING"; // task_->state() to string?
    // Map stats appropriately here
    snapshot.startTimeMs = stats.executionStartTimeMs;
    snapshot.elapsedMs = snapshot.startTimeMs > 0
        ? (latest->timestampMs - snapshot.startTimeMs)
        : 0;

    snapshot.numTotalDrivers = stats.numTotalDrivers;
    snapshot.numRunningDrivers = stats.numRunningDrivers;
    int blockedSum = 0;
    for (const auto& [reason, count] : stats.numBlockedDrivers) {
      blockedSum += count;
    }
    snapshot.numBlockedDrivers = blockedSum;
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
