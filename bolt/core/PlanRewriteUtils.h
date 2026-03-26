/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include "bolt/core/PlanNode.h"

namespace bytedance::bolt::core {

class PlanRewriteUtils {
 public:
  // Clone a node but give it a new set of sources.
  // We can use plan serde to serialize -> modify sources -> deserialize,
  // or use node-specific cloning if needed.
  static std::shared_ptr<const PlanNode> cloneWithNewSources(
      const std::shared_ptr<const PlanNode>& node,
      const std::vector<std::shared_ptr<const PlanNode>>& newSources);

  // Transform a plan by applying a rewrite function recursively.
  // The rewriter should return a new node, or the same node if no change.
  // Using a bottom-up approach (post-order traversal).
  static std::shared_ptr<const PlanNode> transformBottomUp(
      const std::shared_ptr<const PlanNode>& node,
      const std::function<std::shared_ptr<const PlanNode>(
          const std::shared_ptr<const PlanNode>&)>& rewriter);
};

} // namespace bytedance::bolt::core
