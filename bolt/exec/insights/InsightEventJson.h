#pragma once

#include <folly/dynamic.h>
#include "bolt/exec/insights/Insight.h"

namespace bytedance::bolt::exec::insights {

folly::dynamic toJson(const InsightScope& scope);
folly::dynamic toJson(const InsightEvent& event);
folly::dynamic toJson(const QuerySnapshot& snapshot);

} // namespace bytedance::bolt::exec::insights
