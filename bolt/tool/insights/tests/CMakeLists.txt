# Copyright (c) ByteDance Ltd. and/or its affiliates.
# SPDX-License-Identifier: Apache-2.0

add_executable(
  bolt_tool_insights_test
  InsightDemoRunnerTest.cpp
)

target_compile_features(bolt_tool_insights_test PRIVATE cxx_std_20)

target_link_libraries(
  bolt_tool_insights_test
  PRIVATE
  bolt_tool_insights
  bolt_exec_insights
  bolt_exec
  bolt_testutils
  GTest::gtest
  GTest::gtest_main
  GTest::gmock
)

add_test(
  NAME bolt_tool_insights_test
  COMMAND bolt_tool_insights_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
