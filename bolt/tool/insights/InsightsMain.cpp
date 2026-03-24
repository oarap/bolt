// Copyright (c) ByteDance Ltd. and/or its affiliates.
// SPDX-License-Identifier: Apache-2.0

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <string>
#include "bolt/tool/insights/InsightDemoRunner.h"

DEFINE_string(command, "", "Command to run: run, snapshot, stream, cancel");
DEFINE_string(scenario, "default", "Scenario to run (for 'run' command)");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_command.empty()) {
    std::cerr << "Usage: " << argv[0] << " --command=<cmd>" << std::endl;
    std::cerr << "Commands: run, snapshot, stream, cancel" << std::endl;
    return 1;
  }

  bytedance::bolt::tool::insights::InsightDemoRunner runner;

  if (FLAGS_command == "run") {
    runner.runScenario(FLAGS_scenario);
  } else if (FLAGS_command == "snapshot") {
    runner.snapshot();
  } else if (FLAGS_command == "stream") {
    runner.stream();
  } else if (FLAGS_command == "cancel") {
    runner.cancel();
  } else {
    std::cerr << "Unknown command: " << FLAGS_command << std::endl;
    return 1;
  }

  return 0;
}
