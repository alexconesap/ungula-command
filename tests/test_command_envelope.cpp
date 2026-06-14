#include <gtest/gtest.h>

#include <ungula/command/command_envelope.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

using namespace ungula::command;

namespace
{
struct SmallPayload {
    uint32_t a;
    uint16_t b;
    uint8_t c;
};

struct FullPayload {
    uint8_t bytes[COMMAND_INLINE_PAYLOAD_MAX];
};
} // namespace

TEST(CommandEnvelope, DefaultState)
{
    CommandEnvelope env;
    EXPECT_EQ(env.id, 0u);
    EXPECT_EQ(env.domain, CommandDomain::Common);
    EXPECT_EQ(env.type, 0u);
    EXPECT_EQ(env.source, CommandSource::Internal);
    EXPECT_EQ(env.target, 0u);
    EXPECT_EQ(env.flags, 0u);
    EXPECT_EQ(env.payloadKind, CommandPayloadKind::None);
    EXPECT_EQ(env.payloadSize, 0u);

    for (uint8_t b : env.inlinePayload) {
        EXPECT_EQ(b, 0u);
    }
}

TEST(CommandEnvelope, SetInlineAndGetInlineRoundTrip)
{
    CommandEnvelope env;
    SmallPayload in{.a = 0xAABBCCDDu, .b = 0x3344u, .c = 0x55u};

    env.setInline(in);

    EXPECT_EQ(env.payloadKind, CommandPayloadKind::InlineBinary);
    EXPECT_EQ(env.payloadSize, sizeof(SmallPayload));

    SmallPayload out{};
    ASSERT_TRUE(env.getInline(out));
    EXPECT_EQ(out.a, in.a);
    EXPECT_EQ(out.b, in.b);
    EXPECT_EQ(out.c, in.c);
}

TEST(CommandEnvelope, GetInlineFailsWhenKindIsNotInline)
{
    CommandEnvelope env;
    env.payloadKind = CommandPayloadKind::None;
    env.payloadSize = static_cast<uint8_t>(sizeof(SmallPayload));

    SmallPayload out{};
    EXPECT_FALSE(env.getInline(out));
}

TEST(CommandEnvelope, GetInlineFailsWhenSizeMismatches)
{
    CommandEnvelope env;
    SmallPayload in{.a = 1, .b = 2, .c = 3};
    env.setInline(in);

    uint32_t wrongType = 0;
    EXPECT_FALSE(env.getInline(wrongType));
}

TEST(CommandEnvelope, InlinePayloadAtMaxSizeWorks)
{
    CommandEnvelope env;
    FullPayload in{};
    for (uint8_t i = 0; i < COMMAND_INLINE_PAYLOAD_MAX; ++i) {
        in.bytes[i] = i;
    }

    env.setInline(in);
    EXPECT_EQ(env.payloadSize, COMMAND_INLINE_PAYLOAD_MAX);
    EXPECT_EQ(env.payloadKind, CommandPayloadKind::InlineBinary);
    EXPECT_EQ(std::memcmp(env.inlinePayload, in.bytes, COMMAND_INLINE_PAYLOAD_MAX), 0);

    FullPayload out{};
    ASSERT_TRUE(env.getInline(out));
    EXPECT_EQ(std::memcmp(out.bytes, in.bytes, COMMAND_INLINE_PAYLOAD_MAX), 0);
}

TEST(CommandEnvelope, IsTriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable<CommandEnvelope>::value);
}
