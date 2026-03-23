#pragma once

#include <deque>
#include <mutex>
#include <optional>
#include "bolt/exec/insights/TaskSample.h"

namespace bytedance::bolt::exec::insights {

class TaskSampleCollector {
 public:
  explicit TaskSampleCollector(int32_t windowSize = 10);

  void addSample(TaskSample sample);
  std::vector<TaskSample> getRecentSamples() const;
  std::optional<TaskSample> getLatestSample() const;

 private:
  const int32_t windowSize_;
  mutable std::mutex mutex_;
  std::deque<TaskSample> samples_;
};

} // namespace bytedance::bolt::exec::insights
