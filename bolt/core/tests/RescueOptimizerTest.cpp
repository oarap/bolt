/*
 * Copyright (c) ByteDance Ltd. and/or its affiliates.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bolt/core/RescueOptimizer.h"
#include <gtest/gtest.h>
#include "bolt/core/PlanNode.h"
#include "bolt/core/QueryConfig.h"
#include "bolt/core/QueryCtx.h"

using namespace bytedance::bolt::core;

class RescueOptimizerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(RescueOptimizerTest, DisabledWhenExplicitlyFalse) {
  std::unordered_map<std::string, std::string> configs;
  configs[QueryConfig::kEnableRescueOptimizer] = "false";
  QueryConfig queryConfig(std::move(configs));

  PlanFragment fragment;
  auto result = RescueOptimizer::optimize(fragment, queryConfig);

  EXPECT_TRUE(result.reports.empty());
}

TEST_F(RescueOptimizerTest, EnabledDoesNothingWhenEmpty) {
  std::unordered_map<std::string, std::string> configs;
  configs[QueryConfig::kEnableRescueOptimizer] = "true";
  QueryConfig queryConfig(std::move(configs));

  PlanFragment fragment;
  auto result = RescueOptimizer::optimize(fragment, queryConfig);

  EXPECT_TRUE(result.reports.empty());
}

TEST_F(RescueOptimizerTest, SortRescueLimitOverOrderBy) {
  std::unordered_map<std::string, std::string> configs;
  configs[QueryConfig::kEnableRescueOptimizer] = "true";
  QueryConfig queryConfig(std::move(configs));

  auto rowType = bytedance::bolt::ROW({"id"}, {bytedance::bolt::BIGINT()});

  std::shared_ptr<bytedance::bolt::connector::ConnectorTableHandle> tableHandle;
  std::unordered_map<
      std::string,
      std::shared_ptr<bytedance::bolt::connector::ColumnHandle>>
      assignments;
  std::shared_ptr<PlanNode> tableScan =
      std::make_shared<TableScanNode>("1", rowType, tableHandle, assignments);

  std::vector<std::shared_ptr<const FieldAccessTypedExpr>> sortingKeys;
  sortingKeys.push_back(std::make_shared<const FieldAccessTypedExpr>(
      bytedance::bolt::BIGINT(), "id"));
  std::vector<SortOrder> sortingOrders = {SortOrder(true, true)};

  std::shared_ptr<PlanNode> orderBy = std::make_shared<OrderByNode>(
      "2", sortingKeys, sortingOrders, false, tableScan);

  std::shared_ptr<PlanNode> limit =
      std::make_shared<LimitNode>("3", 0, 10, false, orderBy);

  PlanFragment fragment(limit);
  auto result = RescueOptimizer::optimize(fragment, queryConfig);

  EXPECT_EQ(result.reports.size(), 1);
  EXPECT_EQ(result.reports[0].ruleName, "SortRescueRule");
  EXPECT_EQ(
      result.reports[0].proofSummary, "Converted Limit over OrderBy to TopN");

  auto root = result.fragment.planNode;
  EXPECT_EQ(root->name(), "TopN");
}
