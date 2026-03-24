#pragma once

#include <cstdint>

namespace bytedance::bolt::exec::insights {

struct InsightOptions {
  int64_t pollIntervalMs{500};
  int32_t historyWindowSize{10};
  int32_t minSamplesForRules{2};
  uint64_t minRowsForSelectivity{10000};
  double lowSelectivityThreshold{0.95};
};

} // namespace bytedance::bolt::exec::insights
