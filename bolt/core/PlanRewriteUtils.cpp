/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bolt/core/PlanRewriteUtils.h"

namespace bytedance::bolt::core {

std::shared_ptr<const PlanNode> PlanRewriteUtils::cloneWithNewSources(
    const std::shared_ptr<const PlanNode>& node,
    const std::vector<std::shared_ptr<const PlanNode>>& newSources) {
  if (!node) {
    return nullptr;
  }

  // If sources are the same (pointer identity), just return the original node
  if (node->sources().size() == newSources.size()) {
    bool allSame = true;
    for (size_t i = 0; i < newSources.size(); ++i) {
      if (node->sources()[i].get() != newSources[i].get()) {
        allSame = false;
        break;
      }
    }
    if (allSame) {
      return node;
    }
  }

  const std::string name(node->name());

  if (name == "Filter") {
    auto filterNode = std::dynamic_pointer_cast<const FilterNode>(node);
    if (filterNode) {
      return std::make_shared<FilterNode>(
          filterNode->id(), filterNode->filter(), newSources[0]);
    }
  } else if (name == "Limit") {
    auto limitNode = std::dynamic_pointer_cast<const LimitNode>(node);
    if (limitNode) {
      return std::make_shared<LimitNode>(
          limitNode->id(),
          limitNode->offset(),
          limitNode->count(),
          limitNode->isPartial(),
          newSources[0]);
    }
  } else if (name == "OrderBy") {
    auto orderByNode = std::dynamic_pointer_cast<const OrderByNode>(node);
    if (orderByNode) {
      return std::make_shared<OrderByNode>(
          orderByNode->id(),
          orderByNode->sortingKeys(),
          orderByNode->sortingOrders(),
          orderByNode->isPartial(),
          newSources[0]);
    }
  } else if (name == "Aggregation") {
    auto aggNode = std::dynamic_pointer_cast<const AggregationNode>(node);
    if (aggNode) {
      if (aggNode->globalGroupingSets().empty() &&
          !aggNode->groupId().has_value()) {
        return std::make_shared<AggregationNode>(
            aggNode->id(),
            aggNode->step(),
            aggNode->groupingKeys(),
            aggNode->preGroupedKeys(),
            aggNode->aggregateNames(),
            aggNode->aggregates(),
            aggNode->ignoreNullKeys(),
            newSources[0],
            aggNode->isSortBased());
      } else {
        return std::make_shared<AggregationNode>(
            aggNode->id(),
            aggNode->step(),
            aggNode->groupingKeys(),
            aggNode->preGroupedKeys(),
            aggNode->aggregateNames(),
            aggNode->aggregates(),
            aggNode->globalGroupingSets(),
            aggNode->groupId(),
            aggNode->ignoreNullKeys(),
            newSources[0],
            aggNode->isSortBased());
      }
    }
  } else if (name == "HashJoin") {
    auto hashJoinNode = std::dynamic_pointer_cast<const HashJoinNode>(node);
    if (hashJoinNode) {
      return std::make_shared<HashJoinNode>(
          hashJoinNode->id(),
          hashJoinNode->joinType(),
          hashJoinNode->isNullAware(),
          hashJoinNode->leftKeys(),
          hashJoinNode->rightKeys(),
          hashJoinNode->filter(),
          newSources[0],
          newSources[1],
          hashJoinNode->outputType());
    }
  }

  // Throw for unimplemented types for now
  throw std::runtime_error(
      "cloneWithNewSources not implemented for node type: " + name);
}

std::shared_ptr<const PlanNode> PlanRewriteUtils::transformBottomUp(
    const std::shared_ptr<const PlanNode>& node,
    const std::function<std::shared_ptr<const PlanNode>(
        const std::shared_ptr<const PlanNode>&)>& rewriter) {
  if (!node) {
    return nullptr;
  }

  bool sourcesChanged = false;
  std::vector<std::shared_ptr<const PlanNode>> newSources;
  newSources.reserve(node->sources().size());

  for (const auto& source : node->sources()) {
    auto newSource = transformBottomUp(source, rewriter);
    newSources.push_back(newSource);
    if (newSource.get() != source.get()) {
      sourcesChanged = true;
    }
  }

  std::shared_ptr<const PlanNode> newNode = node;
  if (sourcesChanged) {
    newNode = cloneWithNewSources(node, newSources);
  }

  return rewriter(newNode);
}

} // namespace bytedance::bolt::core
