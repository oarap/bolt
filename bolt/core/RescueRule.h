/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <optional>
#include "bolt/core/PlanNode.h"
#include "bolt/core/RescueOptimizer.h"

namespace bytedance::bolt::core {

class RescueRule {
 public:
  virtual ~RescueRule() = default;

  // The name of the rule for reporting
  virtual std::string name() const = 0;

  // Returns the optimized node and an optional report if the rule fires
  virtual std::
      pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>>
      apply(const std::shared_ptr<const PlanNode>& node) const = 0;
};

class SortRescueRule : public RescueRule {
 public:
  std::string name() const override {
    return "SortRescueRule";
  }

  std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>> apply(
      const std::shared_ptr<const PlanNode>& node) const override;
};

class OuterToInnerJoinRule : public RescueRule {
 public:
  std::string name() const override {
    return "OuterToInnerJoinRule";
  }

  std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>> apply(
      const std::shared_ptr<const PlanNode>& node) const override;
};

class JoinToSemiJoinRule : public RescueRule {
 public:
  std::string name() const override {
    return "JoinToSemiJoinRule";
  }

  std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>> apply(
      const std::shared_ptr<const PlanNode>& node) const override;
};

class LeftJoinEliminationRule : public RescueRule {
 public:
  std::string name() const override {
    return "LeftJoinEliminationRule";
  }

  std::pair<std::shared_ptr<const PlanNode>, std::optional<RescueReport>> apply(
      const std::shared_ptr<const PlanNode>& node) const override;
};

} // namespace bytedance::bolt::core
