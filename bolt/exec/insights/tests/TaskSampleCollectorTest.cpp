#include "bolt/exec/insights/TaskSampleCollector.h"
#include <gtest/gtest.h>

using namespace bytedance::bolt::exec::insights;

TEST(TaskSampleCollectorTest, EmptyCollector) {
  TaskSampleCollector collector(5);
  EXPECT_FALSE(collector.getLatestSample().has_value());
  EXPECT_TRUE(collector.getRecentSamples().empty());
}

TEST(TaskSampleCollectorTest, AddAndRetrieve) {
  TaskSampleCollector collector(2);

  TaskSample s1;
  s1.timestampMs = 100;
  collector.addSample(s1);

  EXPECT_TRUE(collector.getLatestSample().has_value());
  EXPECT_EQ(collector.getLatestSample()->timestampMs, 100);
  EXPECT_EQ(collector.getRecentSamples().size(), 1);

  TaskSample s2;
  s2.timestampMs = 200;
  collector.addSample(s2);

  EXPECT_EQ(collector.getLatestSample()->timestampMs, 200);
  EXPECT_EQ(collector.getRecentSamples().size(), 2);

  // Exceed window size
  TaskSample s3;
  s3.timestampMs = 300;
  collector.addSample(s3);

  EXPECT_EQ(collector.getLatestSample()->timestampMs, 300);
  auto recent = collector.getRecentSamples();
  EXPECT_EQ(recent.size(), 2);
  EXPECT_EQ(recent[0].timestampMs, 200);
  EXPECT_EQ(recent[1].timestampMs, 300);
}
