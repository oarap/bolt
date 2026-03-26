/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bolt/core/PlanFragment.h"
#include "bolt/core/QueryConfig.h"

namespace bytedance::bolt::core {

struct RescueReport {
  std::string ruleName;
  std::string status;
  std::string proofSummary;
  std::vector<core::PlanNodeId> affectedNodeIds;
  std::string beforeFragmentString;
  std::string afterFragmentString;
};

struct RescueOptimizationResult {
  core::PlanFragment fragment;
  std::vector<RescueReport> reports;
};

class RescueOptimizer {
 public:
  static RescueOptimizationResult optimize(
      const core::PlanFragment& fragment,
      const core::QueryConfig& queryConfig);
};

} // namespace bytedance::bolt::core
