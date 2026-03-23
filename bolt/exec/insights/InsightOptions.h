#pragma once

#include <cstdint>

namespace bolt::exec::insights {

struct InsightOptions {
  int64_t pollIntervalMs{500};
  int32_t historyWindowSize{10};
  int32_t minSamplesForRules{2};
};

} // namespace bolt::exec::insights
