// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>
#include "bolt/exec/insights/Insight.h"

namespace bytedance::bolt::tool::insights {

class PolicyAgent {
 public:
  enum class Action {
    kContinue,
    kCancel,
    kRerun
  };

  struct PolicyDecision {
    Action action;
    std::string reason;
  };

  PolicyDecision evaluate(const std::vector<exec::insights::InsightEvent>& events);
};

} // namespace bytedance::bolt::tool::insights
