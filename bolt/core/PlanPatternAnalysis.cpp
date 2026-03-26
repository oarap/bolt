/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bolt/core/PlanPatternAnalysis.h"

namespace bytedance::bolt::core {

bool PlanPatternAnalysis::isLimitOverOrderBy(const core::PlanNode* node) {
  if (!node || node->name() != "Limit") {
    return false;
  }

  if (node->sources().empty() || !node->sources()[0]) {
    return false;
  }

  return node->sources()[0]->name() == "OrderBy";
}

bool PlanPatternAnalysis::isParentOrderInsensitive(
    const core::PlanNode* parentNode) {
  if (!parentNode) {
    return false; // Conservatively assume top-level is order-sensitive
  }

  // For V1, we only handle a few clear order-insensitive parents.
  // In the demo example, COUNT(*) over ORDER BY. So aggregation is one.
  const std::string name(parentNode->name());

  return name == "Aggregation" || name == "HashJoin" ||
      name == "NestedLoopJoin" || name == "Filter";
}

bool PlanPatternAnalysis::isNullRejecting(
    const core::PlanNode* filterNode,
    const std::vector<std::string>& rightSideColumns) {
  if (!filterNode || filterNode->name() != "Filter") {
    return false;
  }

  // A simplistic approach for v1:
  // If the filter has an equality or comparison on any right-side column,
  // we consider it null-rejecting (because NULL = X is NULL which filters out).
  // In a real optimizer we'd traverse the expression tree.

  // For v1 we can just check if any rightSideColumn appears in the expression
  // and isn't guarded by IS NULL. We'll return false for now to be safe until
  // we implement proper expression traversal.
  return false;
}

bool PlanPatternAnalysis::isHashJoin(const core::PlanNode* node) {
  return node && node->name() == "HashJoin";
}

} // namespace bytedance::bolt::core
