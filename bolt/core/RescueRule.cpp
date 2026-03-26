/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bolt/core/RescueRule.h"
#include "bolt/core/PlanPatternAnalysis.h"
#include "bolt/core/PlanRewriteUtils.h"

namespace bytedance::bolt::core {

std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>>
SortRescueRule::apply(const std::shared_ptr<const PlanNode>& node) const {
  if (!node) {
    return {node, std::nullopt};
  }

  // Case 1: Limit(OrderBy(X)) -> TopN(X)
  if (PlanPatternAnalysis::isLimitOverOrderBy(node.get())) {
    auto limitNode = std::dynamic_pointer_cast<const LimitNode>(node);
    auto orderByNode =
        std::dynamic_pointer_cast<const OrderByNode>(node->sources()[0]);

    if (limitNode && orderByNode && limitNode->offset() == 0) {
      // Create TopN
      auto topN = std::make_shared<TopNNode>(
          limitNode->id(),
          orderByNode->sortingKeys(),
          orderByNode->sortingOrders(),
          limitNode->count(),
          limitNode->isPartial() ||
              orderByNode->isPartial(), // Handle partial flag
          orderByNode->sources()[0]);

      RescueReport report;
      report.ruleName = name();
      report.status = "Applied";
      report.proofSummary = "Converted Limit over OrderBy to TopN";
      report.affectedNodeIds = {limitNode->id(), orderByNode->id()};

      return {topN, report};
    }
  }

  // Case 2: Dead sort elimination
  // If the current node is an order-insensitive node (e.g. Aggregation),
  // and its source is an OrderBy, we can eliminate the OrderBy.
  if (PlanPatternAnalysis::isParentOrderInsensitive(node.get())) {
    if (node->sources().size() == 1 && node->sources()[0] &&
        node->sources()[0]->name() == "OrderBy") {
      auto orderByNode =
          std::dynamic_pointer_cast<const OrderByNode>(node->sources()[0]);
      if (orderByNode) {
        // We replace the sources of the current node to skip the OrderBy
        std::vector<std::shared_ptr<const PlanNode>> newSources = {
            orderByNode->sources()[0]};
        auto newNode = PlanRewriteUtils::cloneWithNewSources(node, newSources);

        RescueReport report;
        report.ruleName = name();
        report.status = "Applied";
        report.proofSummary =
            "Eliminated dead OrderBy under order-insensitive parent";
        report.affectedNodeIds = {orderByNode->id()};

        return {newNode, report};
      }
    }
  }

  return {node, std::nullopt};
}

std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>>
OuterToInnerJoinRule::apply(const std::shared_ptr<const PlanNode>& node) const {
  if (!node || node->name() != "Filter") {
    return {node, std::nullopt};
  }

  // Check if child is HashJoin
  if (node->sources().size() == 1 && node->sources()[0]) {
    auto hashJoinNode =
        std::dynamic_pointer_cast<const HashJoinNode>(node->sources()[0]);
    if (hashJoinNode && hashJoinNode->joinType() == JoinType::kLeft) {
      // Determine right side columns from join type
      // For V1, we'll just check if it's null-rejecting without full column
      // info yet
      std::vector<std::string> dummyRightSideCols;

      if (PlanPatternAnalysis::isNullRejecting(
              node.get(), dummyRightSideCols)) {
        // Rewrite to Inner Join
        auto innerJoinNode = std::make_shared<HashJoinNode>(
            hashJoinNode->id(),
            JoinType::kInner,
            hashJoinNode->isNullAware(),
            hashJoinNode->leftKeys(),
            hashJoinNode->rightKeys(),
            hashJoinNode->filter(),
            hashJoinNode->sources()[0],
            hashJoinNode->sources()[1],
            hashJoinNode->outputType());

        std::vector<std::shared_ptr<const PlanNode>> newSources = {
            innerJoinNode};
        auto newFilterNode =
            PlanRewriteUtils::cloneWithNewSources(node, newSources);

        RescueReport report;
        report.ruleName = name();
        report.status = "Applied";
        report.proofSummary =
            "Converted Left Join to Inner Join due to null-rejecting filter";
        report.affectedNodeIds = {hashJoinNode->id()};

        return {newFilterNode, report};
      }
    }
  }

  return {node, std::nullopt};
}

std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>>
JoinToSemiJoinRule::apply(const std::shared_ptr<const PlanNode>& node) const {
  // Check if it's an inner join and its parent only uses left columns and does
  // distinct This is a complex pattern requiring analyzing the parent node too,
  // which our bottom-up traversal might not give us easily unless we look at
  // children of distinct nodes. We'll match Distinct -> HashJoin(Inner) where
  // right side is just for existence.

  if (!node || node->name() != "Aggregation") {
    return {node, std::nullopt};
  }

  // Check if it's a distinct aggregation
  auto aggNode = std::dynamic_pointer_cast<const AggregationNode>(node);
  if (aggNode &&
      aggNode->aggregates().empty()) { // Distinct usually has no aggregate
                                       // functions, just grouping keys

    // Check if child is Inner Join
    if (aggNode->sources().size() == 1 && aggNode->sources()[0]) {
      auto hashJoinNode =
          std::dynamic_pointer_cast<const HashJoinNode>(aggNode->sources()[0]);

      if (hashJoinNode && hashJoinNode->joinType() == JoinType::kInner) {
        // Ensure all grouping keys come from the left side of the join
        bool allKeysFromLeft = true;
        auto leftOutputType = hashJoinNode->sources()[0]->outputType();
        for (const auto& key : aggNode->groupingKeys()) {
          if (!leftOutputType->containsChild(key->name())) {
            allKeysFromLeft = false;
            break;
          }
        }

        if (allKeysFromLeft) {
          auto semiJoinNode = std::make_shared<HashJoinNode>(
              hashJoinNode->id(),
              JoinType::kLeftSemiFilter, // Use LeftSemiFilter
              hashJoinNode->isNullAware(),
              hashJoinNode->leftKeys(),
              hashJoinNode->rightKeys(),
              hashJoinNode->filter(),
              hashJoinNode->sources()[0],
              hashJoinNode->sources()[1],
              hashJoinNode
                  ->outputType()); // Note: LeftSemi output type might be
                                   // different from Inner, we'd need to adapt

          std::vector<std::shared_ptr<const PlanNode>> newSources = {
              semiJoinNode};
          auto newAggNode =
              PlanRewriteUtils::cloneWithNewSources(node, newSources);

          RescueReport report;
          report.ruleName = name();
          report.status = "Applied";
          report.proofSummary =
              "Converted Inner Join to Left Semi Join under Distinct";
          report.affectedNodeIds = {hashJoinNode->id()};

          return {newAggNode, report};
        }
      }
    }
  }

  return {node, std::nullopt};
}

std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>>
LeftJoinEliminationRule::apply(
    const std::shared_ptr<const PlanNode>& node) const {
  if (!node || node->name() != "Aggregation") {
    return {node, std::nullopt};
  }

  // Check if it's a distinct aggregation
  auto aggNode = std::dynamic_pointer_cast<const AggregationNode>(node);
  if (aggNode && aggNode->aggregates().empty()) {
    // Check if child is Left Join
    if (aggNode->sources().size() == 1 && aggNode->sources()[0]) {
      auto hashJoinNode =
          std::dynamic_pointer_cast<const HashJoinNode>(aggNode->sources()[0]);

      if (hashJoinNode && hashJoinNode->joinType() == JoinType::kLeft) {
        // Ensure all grouping keys come from the left side of the join
        bool allKeysFromLeft = true;
        auto leftOutputType = hashJoinNode->sources()[0]->outputType();
        for (const auto& key : aggNode->groupingKeys()) {
          if (!leftOutputType->containsChild(key->name())) {
            allKeysFromLeft = false;
            break;
          }
        }

        if (allKeysFromLeft) {
          // Eliminate the join completely by routing directly to its left child
          std::vector<std::shared_ptr<const PlanNode>> newSources = {
              hashJoinNode->sources()[0]};
          auto newAggNode =
              PlanRewriteUtils::cloneWithNewSources(node, newSources);

          RescueReport report;
          report.ruleName = name();
          report.status = "Applied";
          report.proofSummary = "Eliminated Left Join under Distinct";
          report.affectedNodeIds = {hashJoinNode->id()};

          return {newAggNode, report};
        }
      }
    }
  }

  return {node, std::nullopt};
}

} // namespace bytedance::bolt::core
