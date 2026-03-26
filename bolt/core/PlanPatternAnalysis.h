/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "bolt/core/PlanNode.h"

namespace bytedance::bolt::core {

class PlanPatternAnalysis {
 public:
  // Is this node an OrderByNode directly followed by a LimitNode?
  // We want to detect shapes like: LimitNode -> OrderByNode
  static bool isLimitOverOrderBy(const core::PlanNode* node);

  // Can we eliminate this OrderByNode because its parent doesn't care about
  // order? Basically checks if parent is something order-insensitive and not a
  // Limit. We'll define a simple version of this for dead sort elimination.
  static bool isParentOrderInsensitive(const core::PlanNode* parentNode);

  // Does this filter reject nulls on any of the provided columns?
  // We need this to determine if a Left Join can be rewritten as an Inner Join.
  // A filter is null-rejecting on a column if evaluating it with NULL returns
  // NULL or false.
  static bool isNullRejecting(
      const core::PlanNode* filterNode,
      const std::vector<std::string>& rightSideColumns);

  // Is this node a HashJoin?
  static bool isHashJoin(const core::PlanNode* node);
};

} // namespace bytedance::bolt::core
