#include <gtest/gtest.h>

#include <ungula/command/command_queue.h>
#include <ungula/command/command_envelope.h>

using namespace ungula::command;

namespace
{
CommandEnvelope makeCmd(uint32_t id, uint16_t type)
{
    CommandEnvelope cmd;
    cmd.id = id;
    cmd.type = type;
    return cmd;
}
} // namespace

TEST(CommandQueue, SubmitTakePreservesFifoOrder)
{
    CommandQueue<CommandEnvelope, 4> q;

    ASSERT_TRUE(q.submit(makeCmd(1, 10)));
    ASSERT_TRUE(q.submit(makeCmd(2, 20)));
    ASSERT_TRUE(q.submit(makeCmd(3, 30)));

    CommandEnvelope out;
    ASSERT_TRUE(q.take(out));
    EXPECT_EQ(out.id, 1u);
    EXPECT_EQ(out.type, 10u);

    ASSERT_TRUE(q.take(out));
    EXPECT_EQ(out.id, 2u);
    EXPECT_EQ(out.type, 20u);

    ASSERT_TRUE(q.take(out));
    EXPECT_EQ(out.id, 3u);
    EXPECT_EQ(out.type, 30u);
}

TEST(CommandQueue, PeekIsNonDestructive)
{
    CommandQueue<CommandEnvelope, 2> q;
    ASSERT_TRUE(q.submit(makeCmd(11, 101)));

    CommandEnvelope peeked;
    ASSERT_TRUE(q.peek(peeked));
    EXPECT_EQ(peeked.id, 11u);

    CommandEnvelope taken;
    ASSERT_TRUE(q.take(taken));
    EXPECT_EQ(taken.id, 11u);
}

TEST(CommandQueue, SubmitFailsWhenFull)
{
    CommandQueue<CommandEnvelope, 2> q;
    ASSERT_TRUE(q.submit(makeCmd(1, 1)));
    ASSERT_TRUE(q.submit(makeCmd(2, 2)));
    EXPECT_TRUE(q.isFull());
    EXPECT_FALSE(q.submit(makeCmd(3, 3)));
}

TEST(CommandQueue, TakeAndPeekFailWhenEmpty)
{
    CommandQueue<CommandEnvelope, 2> q;
    CommandEnvelope out;
    EXPECT_TRUE(q.isEmpty());
    EXPECT_FALSE(q.peek(out));
    EXPECT_FALSE(q.take(out));
}

TEST(CommandQueue, CountCapacityAndClear)
{
    CommandQueue<CommandEnvelope, 3> q;
    EXPECT_EQ(q.capacity(), 3u);
    EXPECT_EQ(q.count(), 0u);

    ASSERT_TRUE(q.submit(makeCmd(1, 1)));
    ASSERT_TRUE(q.submit(makeCmd(2, 2)));
    EXPECT_EQ(q.count(), 2u);
    EXPECT_FALSE(q.isEmpty());

    q.clear();
    EXPECT_EQ(q.count(), 0u);
    EXPECT_TRUE(q.isEmpty());
    EXPECT_FALSE(q.isFull());
}
