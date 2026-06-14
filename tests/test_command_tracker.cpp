#include <gtest/gtest.h>

#include <ungula/command/command_tracker.h>

using namespace ungula::command;

TEST(CommandTracker, BeginAndCompleteRoundTrip)
{
    CommandTracker<4> tracker;

    ASSERT_TRUE(tracker.begin(100, CommandDomain::Project, 7, CommandSource::Rest, 2, 1234));
    EXPECT_EQ(tracker.pending(), 1u);

    CommandTracker<4>::Entry out;
    ASSERT_TRUE(tracker.complete(100, out));

    EXPECT_EQ(out.id, 100u);
    EXPECT_EQ(out.domain, CommandDomain::Project);
    EXPECT_EQ(out.type, 7u);
    EXPECT_EQ(out.source, CommandSource::Rest);
    EXPECT_EQ(out.target, 2u);
    EXPECT_EQ(out.startedMs, 1234u);
    EXPECT_TRUE(out.active);

    EXPECT_EQ(tracker.pending(), 0u);
}

TEST(CommandTracker, BeginFailsWhenFull)
{
    CommandTracker<2> tracker;
    ASSERT_TRUE(tracker.begin(1, CommandDomain::Common, 1, CommandSource::Ui, 0, 0));
    ASSERT_TRUE(tracker.begin(2, CommandDomain::Common, 1, CommandSource::Ui, 0, 0));
    EXPECT_FALSE(tracker.begin(3, CommandDomain::Common, 1, CommandSource::Ui, 0, 0));
}

TEST(CommandTracker, CompleteUnknownIdReturnsFalse)
{
    CommandTracker<2> tracker;
    ASSERT_TRUE(tracker.begin(11, CommandDomain::Common, 1, CommandSource::Ui, 0, 0));

    CommandTracker<2>::Entry out;
    EXPECT_FALSE(tracker.complete(12, out));
    EXPECT_EQ(tracker.pending(), 1u);
}

TEST(CommandTracker, TakeExpiredUsesGreaterOrEqualBoundary)
{
    CommandTracker<4> tracker;
    ASSERT_TRUE(tracker.begin(10, CommandDomain::Project, 2, CommandSource::Node, 3, 1000));

    CommandTracker<4>::Entry out;
    EXPECT_FALSE(tracker.takeExpired(1499, 500, out));
    EXPECT_TRUE(tracker.takeExpired(1500, 500, out));
    EXPECT_EQ(out.id, 10u);
    EXPECT_EQ(tracker.pending(), 0u);
}

TEST(CommandTracker, HasPendingMatchesDomainTypeTarget)
{
    CommandTracker<4> tracker;
    ASSERT_TRUE(tracker.begin(1, CommandDomain::Project, 50, CommandSource::Ui, 9, 0));

    EXPECT_TRUE(tracker.hasPending(CommandDomain::Project, 50, 9));
    EXPECT_FALSE(tracker.hasPending(CommandDomain::Project, 51, 9));
    EXPECT_FALSE(tracker.hasPending(CommandDomain::Project, 50, 8));
    EXPECT_FALSE(tracker.hasPending(CommandDomain::Common, 50, 9));
}

TEST(CommandTracker, CancelClearsOnlyMatchingEntries)
{
    CommandTracker<6> tracker;
    ASSERT_TRUE(tracker.begin(1, CommandDomain::Project, 10, CommandSource::Ui, 1, 0));
    ASSERT_TRUE(tracker.begin(2, CommandDomain::Project, 10, CommandSource::Ui, 1, 1));
    ASSERT_TRUE(tracker.begin(3, CommandDomain::Project, 11, CommandSource::Ui, 1, 2));
    ASSERT_TRUE(tracker.begin(4, CommandDomain::Project, 10, CommandSource::Ui, 2, 3));
    ASSERT_TRUE(tracker.begin(5, CommandDomain::Common, 10, CommandSource::Ui, 1, 4));

    tracker.cancel(CommandDomain::Project, 10, 1);

    EXPECT_FALSE(tracker.hasPending(CommandDomain::Project, 10, 1));
    EXPECT_TRUE(tracker.hasPending(CommandDomain::Project, 11, 1));
    EXPECT_TRUE(tracker.hasPending(CommandDomain::Project, 10, 2));
    EXPECT_TRUE(tracker.hasPending(CommandDomain::Common, 10, 1));
}

TEST(CommandTracker, ClearResetsAll)
{
    CommandTracker<3> tracker;
    ASSERT_TRUE(tracker.begin(1, CommandDomain::Common, 1, CommandSource::Internal, 0, 0));
    ASSERT_TRUE(tracker.begin(2, CommandDomain::Project, 2, CommandSource::Node, 1, 0));
    EXPECT_EQ(tracker.pending(), 2u);

    tracker.clear();
    EXPECT_EQ(tracker.pending(), 0u);
    EXPECT_FALSE(tracker.hasPending(CommandDomain::Common, 1, 0));
    EXPECT_FALSE(tracker.hasPending(CommandDomain::Project, 2, 1));
}

TEST(CommandTracker, DuplicateAckAfterCompleteIsIgnored)
{
    CommandTracker<2> tracker;
    ASSERT_TRUE(tracker.begin(55, CommandDomain::Project, 9, CommandSource::Cloud, 4, 10));

    CommandTracker<2>::Entry out;
    ASSERT_TRUE(tracker.complete(55, out));
    EXPECT_FALSE(tracker.complete(55, out));
}

TEST(CommandTracker, TimeoutFlowThenLateAck)
{
    CommandTracker<2> tracker;
    ASSERT_TRUE(tracker.begin(99, CommandDomain::Project, 22, CommandSource::Mqtt, 7, 100));

    CommandTracker<2>::Entry expired;
    ASSERT_TRUE(tracker.takeExpired(1700, 1500, expired));
    EXPECT_EQ(expired.id, 99u);

    CommandTracker<2>::Entry out;
    EXPECT_FALSE(tracker.complete(99, out));
}

TEST(CommandTracker, CapacityIsCompileTimeConstant)
{
    CommandTracker<5> tracker;
    EXPECT_EQ(tracker.capacity(), 5u);
}
