#pragma once

#include <memory>
#include "bolt/exec/Task.h"

namespace bytedance::bolt::exec::insights {

struct TaskSample {
  int64_t timestampMs;
  TaskStats taskStats;
};

} // namespace bytedance::bolt::exec::insights
