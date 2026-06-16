#include <gtest/gtest.h>

#include <ungula/command/command_types.h>

#include <cstdint>

using namespace ungula::command;

TEST(CommandTypes, CommandSourceValuesAreStable)
{
    EXPECT_EQ(static_cast<uint8_t>(CommandSource::Internal), 0);
    EXPECT_EQ(static_cast<uint8_t>(CommandSource::Ui), 1);
    EXPECT_EQ(static_cast<uint8_t>(CommandSource::Rest), 2);
    EXPECT_EQ(static_cast<uint8_t>(CommandSource::Node), 3);
    EXPECT_EQ(static_cast<uint8_t>(CommandSource::Cloud), 4);
    EXPECT_EQ(static_cast<uint8_t>(CommandSource::Mqtt), 5);
}

TEST(CommandTypes, CommandDomainValuesAreStable)
{
    EXPECT_EQ(static_cast<uint8_t>(CommandDomain::Common), 0);
    EXPECT_EQ(static_cast<uint8_t>(CommandDomain::Project), 1);
}

TEST(CommandTypes, CommandPayloadKindValuesAreStable)
{
    EXPECT_EQ(static_cast<uint8_t>(CommandPayloadKind::None), 0);
    EXPECT_EQ(static_cast<uint8_t>(CommandPayloadKind::InlineBinary), 1);
    EXPECT_EQ(static_cast<uint8_t>(CommandPayloadKind::ExternalRawText), 2);
    EXPECT_EQ(static_cast<uint8_t>(CommandPayloadKind::ExternalBinary), 3);
}

TEST(CommandTypes, CommandResultToString)
{
    EXPECT_STREQ(toString(CommandResult::Accepted), "ACCEPTED");
    EXPECT_STREQ(toString(CommandResult::RejectedInvalidState), "REJECTED_INVALID_STATE");
    EXPECT_STREQ(toString(CommandResult::RejectedFaultActive), "REJECTED_FAULT_ACTIVE");
    EXPECT_STREQ(toString(CommandResult::RejectedBusy), "REJECTED_BUSY");
    EXPECT_STREQ(toString(CommandResult::RejectedLimit), "REJECTED_LIMIT");
    EXPECT_STREQ(toString(CommandResult::RejectedUnsupported), "REJECTED_UNSUPPORTED");
    EXPECT_STREQ(toString(CommandResult::Timeout), "TIMEOUT");
    EXPECT_STREQ(toString(static_cast<CommandResult>(255)), "UNKNOWN");
}

TEST(CommandTypes, CommandSubmitResultToString)
{
    EXPECT_STREQ(toString(CommandSubmitResult::Accepted), "ACCEPTED");
    EXPECT_STREQ(toString(CommandSubmitResult::RejectedInvalidState), "REJECTED_INVALID_STATE");
    EXPECT_STREQ(toString(CommandSubmitResult::RejectedBusy), "REJECTED_BUSY");
    EXPECT_STREQ(toString(CommandSubmitResult::RejectedDuplicate), "REJECTED_DUPLICATE");
    EXPECT_STREQ(toString(CommandSubmitResult::RejectedQueueFull), "REJECTED_QUEUE_FULL");
    EXPECT_STREQ(toString(CommandSubmitResult::RejectedUnsupported), "REJECTED_UNSUPPORTED");
    EXPECT_STREQ(toString(static_cast<CommandSubmitResult>(255)), "UNKNOWN");
}
