#include <gtest/gtest.h>
#include "bolt/common/time/Timer.h"
#include "bolt/core/QueryConfig.h"
#include "bolt/core/RescueOptimizer.h"
#include "bolt/exec/tests/utils/OperatorTestBase.h"
#include "bolt/exec/tests/utils/PlanBuilder.h"

using namespace bytedance::bolt;
using namespace bytedance::bolt::exec;
using namespace bytedance::bolt::exec::test;
using namespace bytedance::bolt::core;

class RescueOptimizerBenchmark : public OperatorTestBase {
 protected:
  void SetUp() override {
    OperatorTestBase::SetUp();
  }

  std::vector<RowVectorPtr> makeData(int32_t numVectors, int32_t vectorSize) {
    std::vector<RowVectorPtr> vectors;
    for (int32_t i = 0; i < numVectors; ++i) {
      vectors.push_back(makeRowVector(
          {makeFlatVector<int64_t>(vectorSize, [](auto row) { return row; }),
           makeFlatVector<int64_t>(
               vectorSize, [](auto row) { return row % 100; }),
           makeFlatVector<double>(
               vectorSize, [](auto row) { return row * 0.1; })}));
    }
    return vectors;
  }

  std::vector<RowVectorPtr> makeJoinDataRight(
      int32_t numVectors,
      int32_t vectorSize,
      int32_t keyModulo = 50) {
    std::vector<RowVectorPtr> vectors;
    for (int32_t i = 0; i < numVectors; ++i) {
      // Massive payload to make reading/hashing expensive
      std::string hugeString(1000, 'x'); // 1000 byte string
      vectors.push_back(makeRowVector(
          {"u0", "u1", "u2"},
          {makeFlatVector<int64_t>(
               vectorSize,
               [&](auto row) { return (i * vectorSize + row) % keyModulo; }),
           makeFlatVector<int64_t>(vectorSize, [](auto row) { return row; }),
           makeFlatVector<StringView>(
               vectorSize, [&](auto row) { return StringView(hugeString); })}));
    }
    return vectors;
  }

  std::vector<RowVectorPtr>
  makeJoinData(int32_t numVectors, int32_t vectorSize, int32_t keyModulo = 50) {
    std::vector<RowVectorPtr> vectors;
    for (int32_t i = 0; i < numVectors; ++i) {
      vectors.push_back(makeRowVector(
          {"c0", "c1"},
          {makeFlatVector<int64_t>(
               vectorSize,
               [&](auto row) { return (i * vectorSize + row) % keyModulo; }),
           makeFlatVector<int64_t>(vectorSize, [](auto row) { return row; })}));
    }
    return vectors;
  }
};

TEST_F(RescueOptimizerBenchmark, SortRescueBenchmark) {
  auto data = makeData(200, 10000); // 2M rows

  auto plan = PlanBuilder()
                  .values(data)
                  .orderBy({"c0 DESC"}, false)
                  .limit(0, 10, false)
                  .planNode();

  auto measureExecution = [&](bool enableOptimizer) {
    auto queryCtx = core::QueryCtx::create(executor_.get());
    std::unordered_map<std::string, std::string> config;
    config[core::QueryConfig::kEnableRescueOptimizer] =
        enableOptimizer ? "true" : "false";
    queryCtx->testingOverrideConfigUnsafe(std::move(config));

    CursorParameters params;
    params.planNode = plan;
    params.queryCtx = queryCtx;
    params.maxDrivers = 1;

    uint64_t startTime = getCurrentTimeMicro();
    auto result = readCursor(params, [](Task*) {});
    uint64_t endTime = getCurrentTimeMicro();

    return (endTime - startTime) / 1000.0;
  };

  std::cout
      << "\n==================================================================\n";
  std::cout << "=== Sort Rescue Benchmark (OrderBy + Limit) ===" << std::endl;
  std::cout
      << "==================================================================\n";

  std::cout << "\n[Original Plan Tree]:\n";
  std::cout << plan->toString(true, true) << std::endl;

  auto queryCtx = core::QueryCtx::create(executor_.get());
  std::unordered_map<std::string, std::string> config;
  config[core::QueryConfig::kEnableRescueOptimizer] = "true";
  queryCtx->testingOverrideConfigUnsafe(std::move(config));

  core::PlanFragment fragment{plan};
  auto optimizationResult =
      core::RescueOptimizer::optimize(fragment, queryCtx->queryConfig());
  std::cout << "\n[Optimized Plan Tree]:\n";
  std::cout << optimizationResult.fragment.planNode->toString(true, true)
            << std::endl;

  double timeWithout = measureExecution(false);
  std::cout << "\nTime without optimizer: " << timeWithout << " ms"
            << std::endl;

  double timeWith = measureExecution(true);
  std::cout << "Time with optimizer: " << timeWith << " ms" << std::endl;

  std::cout << "Speedup: " << timeWithout / timeWith << "x\n\n" << std::endl;
}

TEST_F(RescueOptimizerBenchmark, JoinToSemiJoinBenchmark) {
  // Scenario: Massive data explosion. Right table has many duplicate keys.
  // Using 30k rows each side with 10 keys -> 90M rows explosion.
  auto leftData = makeJoinData(30, 1000, 10);
  auto rightData = makeJoinDataRight(30, 1000, 10);

  auto planIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();

  // MUST use aggregation to trigger the rule: Distinct -> Inner Join
  auto plan =
      PlanBuilder(planIdGenerator)
          .values(leftData)
          .hashJoin(
              {"c0"},
              {"u0"},
              PlanBuilder(planIdGenerator).values(rightData).planNode(),
              "",
              {"c0", "c1"}, // Only selecting left columns
              core::JoinType::kInner)
          .aggregation(
              {"c0", "c1"}, {}, {}, core::AggregationNode::Step::kSingle, false)
          .planNode();

  auto measureExecution = [&](bool enableOptimizer) {
    auto queryCtx = core::QueryCtx::create(executor_.get());
    std::unordered_map<std::string, std::string> config;
    config[core::QueryConfig::kEnableRescueOptimizer] =
        enableOptimizer ? "true" : "false";
    queryCtx->testingOverrideConfigUnsafe(std::move(config));

    CursorParameters params;
    params.planNode = plan;
    params.queryCtx = queryCtx;
    params.maxDrivers = 1;

    uint64_t startTime = getCurrentTimeMicro();
    auto result = readCursor(params, [](Task*) {});
    uint64_t endTime = getCurrentTimeMicro();

    return (endTime - startTime) / 1000.0;
  };

  std::cout
      << "\n==================================================================\n";
  std::cout << "=== Join to Semi-Join Explosion Rescue ===" << std::endl;
  std::cout
      << "==================================================================\n";

  std::cout << "\n[Original Plan Tree]:\n";
  std::cout << plan->toString(true, true) << std::endl;

  auto queryCtx = core::QueryCtx::create(executor_.get());
  std::unordered_map<std::string, std::string> config;
  config[core::QueryConfig::kEnableRescueOptimizer] = "true";
  queryCtx->testingOverrideConfigUnsafe(std::move(config));

  core::PlanFragment fragment{plan};
  auto optimizationResult =
      core::RescueOptimizer::optimize(fragment, queryCtx->queryConfig());
  std::cout << "\n[Optimized Plan Tree]:\n";
  std::cout << optimizationResult.fragment.planNode->toString(true, true)
            << std::endl;

  double timeWith = measureExecution(true);
  std::cout << "\nTime with optimizer (SemiJoin): " << timeWith << " ms"
            << std::endl;

  double timeWithout = measureExecution(false);
  std::cout << "Time without optimizer (Explosion): " << timeWithout << " ms"
            << std::endl;

  std::cout << "Speedup: " << timeWithout / timeWith << "x\n\n" << std::endl;
}

TEST_F(RescueOptimizerBenchmark, LeftJoinEliminationBenchmark) {
  // Scenario: Joining a fact table with a massive, wide dimension table
  // but we don't actually need any data from the dimension table.
  // Keys are mostly unique to avoid explosion, but hash table is huge.
  auto leftData = makeJoinData(100, 10000, 1000000); // 1M rows, unique keys
  auto rightData = makeJoinDataRight(
      100, 10000, 1000000); // 1M rows, 1000-byte string payloads

  auto planIdGenerator = std::make_shared<core::PlanNodeIdGenerator>();

  // MUST use aggregation to trigger the rule: Distinct -> Left Join
  auto plan =
      PlanBuilder(planIdGenerator)
          .values(leftData)
          .hashJoin(
              {"c0"},
              {"u0"},
              PlanBuilder(planIdGenerator).values(rightData).planNode(),
              "",
              {"c0", "c1"}, // Only selecting left columns!
              core::JoinType::kLeft)
          .aggregation(
              {"c0", "c1"}, {}, {}, core::AggregationNode::Step::kSingle, false)
          .planNode();

  auto measureExecution = [&](bool enableOptimizer) {
    auto queryCtx = core::QueryCtx::create(executor_.get());
    std::unordered_map<std::string, std::string> config;
    config[core::QueryConfig::kEnableRescueOptimizer] =
        enableOptimizer ? "true" : "false";
    queryCtx->testingOverrideConfigUnsafe(std::move(config));

    CursorParameters params;
    params.planNode = plan;
    params.queryCtx = queryCtx;
    params.maxDrivers = 1;

    uint64_t startTime = getCurrentTimeMicro();
    auto result = readCursor(params, [](Task*) {});
    uint64_t endTime = getCurrentTimeMicro();

    return (endTime - startTime) / 1000.0;
  };

  std::cout
      << "\n==================================================================\n";
  std::cout << "=== Left Join Elimination Benchmark (Massive Right Table) ==="
            << std::endl;
  std::cout
      << "==================================================================\n";

  std::cout << "\n[Original Plan Tree]:\n";
  std::cout << plan->toString(true, true) << std::endl;

  auto queryCtx = core::QueryCtx::create(executor_.get());
  std::unordered_map<std::string, std::string> config;
  config[core::QueryConfig::kEnableRescueOptimizer] = "true";
  queryCtx->testingOverrideConfigUnsafe(std::move(config));

  core::PlanFragment fragment{plan};
  auto optimizationResult =
      core::RescueOptimizer::optimize(fragment, queryCtx->queryConfig());
  std::cout << "\n[Optimized Plan Tree]:\n";
  std::cout << optimizationResult.fragment.planNode->toString(true, true)
            << std::endl;

  double timeWith = measureExecution(true);
  std::cout << "\nTime with optimizer: " << timeWith << " ms" << std::endl;

  double timeWithout = measureExecution(false);
  std::cout << "Time without optimizer: " << timeWithout << " ms" << std::endl;

  std::cout << "Speedup: " << timeWithout / timeWith << "x\n\n" << std::endl;
}
