/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bolt/core/RescueOptimizer.h"
#include "bolt/core/PlanRewriteUtils.h"
#include "bolt/core/RescueRule.h"

namespace bytedance::bolt::core {

RescueOptimizationResult RescueOptimizer::optimize(
    const core::PlanFragment& fragment,
    const core::QueryConfig& queryConfig) {
  RescueOptimizationResult result;
  result.fragment = fragment;

  if (!queryConfig.enableRescueOptimizer() || !fragment.planNode) {
    return result;
  }

  std::vector<std::shared_ptr<RescueRule>> rules = {
      std::make_shared<SortRescueRule>(),
      std::make_shared<OuterToInnerJoinRule>(),
      std::make_shared<JoinToSemiJoinRule>(),
      std::make_shared<LeftJoinEliminationRule>()};

  auto currentPlan = fragment.planNode;

  for (const auto& rule : rules) {
    currentPlan = PlanRewriteUtils::transformBottomUp(
        currentPlan,
        [&rule, &result](const std::shared_ptr<const PlanNode>& node) {
          auto [newNode, report] = rule->apply(node);
          if (report) {
            result.reports.push_back(*report);
          }
          return newNode;
        });
  }

  result.fragment.planNode = currentPlan;
  return result;
}

} // namespace bytedance::bolt::core
