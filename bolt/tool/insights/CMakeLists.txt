# Copyright (c) ByteDance Ltd. and/or its affiliates.
# SPDX-License-Identifier: Apache-2.0

add_library(
  bolt_tool_insights
  PolicyAgent.cpp
  InsightDemoRunner.cpp
)

target_link_libraries(
  bolt_tool_insights
  PUBLIC
  bolt_exec_insights
  bolt_exec
  bolt_common_base
)

add_executable(
  bolt_insights_tool
  InsightsMain.cpp
)

target_link_libraries(
  bolt_insights_tool
  PRIVATE
  bolt_tool_insights
  gflags
  glog::glog
)

if(BOLT_BUILD_TESTING)
  add_subdirectory(tests)
endif()
