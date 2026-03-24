// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include "bolt/exec/insights/TaskInsightSession.h"
#include "bolt/tool/insights/PolicyAgent.h"

namespace bytedance::bolt::tool::insights {

class InsightDemoRunner {
 public:
  InsightDemoRunner();

  void runScenario(const std::string& scenarioName);

  // Commands for CLI
  void snapshot();
  void stream();
  void cancel();

 private:
  void runWithPolicy(
      const std::shared_ptr<exec::insights::TaskInsightSession>& session);

  PolicyAgent agent_;
};

} // namespace bytedance::bolt::tool::insights
