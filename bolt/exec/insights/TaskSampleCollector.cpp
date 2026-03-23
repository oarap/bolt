#include "bolt/exec/insights/TaskSampleCollector.h"

namespace bytedance::bolt::exec::insights {

TaskSampleCollector::TaskSampleCollector(int32_t windowSize)
    : windowSize_(windowSize) {}

void TaskSampleCollector::addSample(TaskSample sample) {
  std::lock_guard<std::mutex> lock(mutex_);
  samples_.push_back(std::move(sample));
  if (samples_.size() > windowSize_) {
    samples_.pop_front();
  }
}

std::vector<TaskSample> TaskSampleCollector::getRecentSamples() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::vector<TaskSample>(samples_.begin(), samples_.end());
}

std::optional<TaskSample> TaskSampleCollector::getLatestSample() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (samples_.empty()) {
    return std::nullopt;
  }
  return samples_.back();
}

} // namespace bytedance::bolt::exec::insights
