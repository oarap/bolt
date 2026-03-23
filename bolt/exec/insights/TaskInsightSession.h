#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "bolt/exec/Task.h"
#include "bolt/exec/insights/Insight.h"
#include "bolt/exec/insights/InsightOptions.h"
#include "bolt/exec/insights/TaskSampleCollector.h"

namespace bytedance::bolt::exec::insights {

class InsightEngine; // Forward declaration

class TaskInsightSession
    : public std::enable_shared_from_this<TaskInsightSession> {
 public:
  static std::shared_ptr<TaskInsightSession> attach(
      const std::shared_ptr<Task>& task,
      InsightOptions options = {},
      SessionMetadata metadata = {});

  ~TaskInsightSession();

  QuerySnapshot snapshot() const;
  std::vector<InsightEvent> poll();
  bytedance::bolt::ContinueFuture cancel();
  void close();

 private:
  TaskInsightSession(
      std::shared_ptr<Task> task,
      InsightOptions options,
      SessionMetadata metadata);

  void doPoll();
  void doClose();

  std::shared_ptr<Task> task_;
  InsightOptions options_;
  SessionMetadata metadata_;

  std::atomic<bool> closed_{false};
  uint64_t nextSequenceId_{0};
  TaskSampleCollector sampleCollector_;

  // Empty engine pointer for Phase 1
  // std::unique_ptr<InsightEngine> engine_;
};

} // namespace bytedance::bolt::exec::insights
