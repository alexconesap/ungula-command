// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// lib_command is header-only. This TU exists only so that any consumer which
// globs lib_command/src/*.cpp force-compiles the headers (and fires their
// static_asserts) here, instead of failing first at a project's point of use.
// No runtime code: the function is never called.

#include "ungula/command/command.h"

namespace ungula::command
{
namespace
{

[[maybe_unused]] void command_selftest()
{
        CommandEnvelope env;
        const uint32_t delayMs = 1000;
        env.setInline(delayMs);
        uint32_t got = 0;
        (void)env.getInline(got);

        CommandQueue<CommandEnvelope, 4> queue;
        (void)queue.submit(env);
        (void)queue.peek(env);
        (void)queue.take(env);
        (void)queue.count();

        CommandTracker<8> tracker;
        (void)tracker.begin(env.id, env.domain, env.type, env.source, env.target, 0);
        CommandTracker<8>::Entry entry;
        (void)tracker.complete(env.id, entry);
        (void)tracker.takeExpired(0, 100, entry);
        (void)tracker.hasPending(env.domain, env.type, env.target);
        tracker.cancel(env.domain, env.type, env.target);

        (void)toString(CommandResult::Accepted);
        (void)toString(SendResult::Sent);
        (void)toString(CommandSubmitResult::Accepted);
}

} // namespace
} // namespace ungula::command
