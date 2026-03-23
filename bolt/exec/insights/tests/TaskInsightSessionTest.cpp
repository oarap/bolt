#include "bolt/exec/insights/TaskInsightSession.h"
#include "bolt/exec/tests/utils/PlanBuilder.h"
#include "bolt/exec/tests/utils/OperatorTestBase.h"
#include "bolt/exec/Task.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt;
using namespace bytedance::bolt::exec;
using namespace bytedance::bolt::exec::test;
using namespace bytedance::bolt::exec::insights;

class TaskInsightSessionTest : public OperatorTestBase {
 protected:
  void SetUp() override {
    OperatorTestBase::SetUp();
  }
};

TEST_F(TaskInsightSessionTest, AttachAndSnapshot) {
  auto plan = PlanBuilder()
                  .values({makeRowVector(ROW({"a"}, {INTEGER()}), 1)})
                  .planFragment();

  auto task = Task::create(
      "task-1",
      std::move(plan),
      0,
      core::QueryCtx::create(executor_.get()),
      Task::ExecutionMode::kParallel);

  SessionMetadata meta;
  meta.queryId = "q1";
  meta.taskId = "t1";

  auto session = TaskInsightSession::attach(task, InsightOptions{}, meta);
  
  // First poll to get a sample
  session->poll();

  auto snapshot = session->snapshot();
  EXPECT_EQ(snapshot.queryId.value(), "q1");
  EXPECT_EQ(snapshot.taskId.value(), "t1");
  EXPECT_EQ(snapshot.taskState, "Running"); // Actually "Running" in Bolt

  // Mock task completion
  task->requestCancel();

  // Poll again, should catch terminal state
  session->poll();
  auto terminalSnapshot = session->snapshot();
  EXPECT_EQ(terminalSnapshot.taskState, "Canceled");
}
