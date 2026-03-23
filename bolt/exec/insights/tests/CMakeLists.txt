add_executable(
  bolt_exec_insights_test
  InsightEventJsonTest.cpp
  TaskSampleCollectorTest.cpp
  TaskInsightSessionTest.cpp
)

target_compile_features(bolt_exec_insights_test PRIVATE cxx_std_20)

target_link_libraries(
  bolt_exec_insights_test
  bolt_exec_insights
  bolt_exec
  bolt_testutils
  GTest::gtest
  GTest::gtest_main
  GTest::gmock
)

add_test(
  NAME bolt_exec_insights_test
  COMMAND bolt_exec_insights_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
