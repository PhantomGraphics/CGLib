#include <gtest/gtest.h>

#include "ScenarioRunner/CommandQueue.h"

#include <thread>

TEST(CommandQueueTest, TakeAllIsFifoAndEmptiesInput)
{
    CommandQueue q;
    q.submit("a");
    q.submit("b");
    auto local = q.takeAll();
    ASSERT_EQ(local.size(), 2u);
    EXPECT_EQ(local.front(), "a");
    local.pop();
    EXPECT_EQ(local.front(), "b");
    EXPECT_TRUE(q.takeAll().empty());
}

TEST(CommandQueueTest, TakeOneReturnsOldestThenFalse)
{
    CommandQueue q;
    std::string cmd;
    EXPECT_FALSE(q.takeOne(cmd));
    q.submit("x");
    q.submit("y");
    ASSERT_TRUE(q.takeOne(cmd));
    EXPECT_EQ(cmd, "x");
    ASSERT_TRUE(q.takeOne(cmd));
    EXPECT_EQ(cmd, "y");
    EXPECT_FALSE(q.takeOne(cmd));
}

TEST(CommandQueueTest, RequeuePutsCommandsBehindNewerOnes)
{
    CommandQueue q;
    q.submit("one");
    q.submit("two");
    auto local = q.takeAll();
    q.submit("late");       // arrives while "one"/"two" are being processed
    std::queue<std::string> rest;
    rest.push("two");
    q.requeue(rest);
    EXPECT_TRUE(rest.empty());
    auto again = q.takeAll();
    ASSERT_EQ(again.size(), 2u);
    EXPECT_EQ(again.front(), "late");
    again.pop();
    EXPECT_EQ(again.front(), "two");
}

TEST(CommandQueueTest, ResponsesAreCollectedOnceInOrder)
{
    CommandQueue q;
    EXPECT_TRUE(q.collectResponses().empty());
    q.respond("OK");
    q.respond("Error:x");
    EXPECT_EQ(q.collectResponses(), (std::vector<std::string>{"OK", "Error:x"}));
    EXPECT_TRUE(q.collectResponses().empty());
}

TEST(CommandQueueTest, ConcurrentSubmitLosesNothing)
{
    CommandQueue q;
    constexpr int kThreads = 4, kEach = 500;
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([&] { for (int i = 0; i < kEach; ++i) q.submit("c"); });
    for (auto& t : ts) t.join();
    EXPECT_EQ(q.takeAll().size(), static_cast<size_t>(kThreads * kEach));
}
