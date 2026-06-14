// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// CommandQueue — a thin, command-oriented facade over the existing fixed-storage
// FIFO (ungula::core::util::Queue). It holds commands that can't be dispatched
// the instant they're submitted (target busy / awaiting a turn). It deliberately
// does NOT duplicate the queue; it just gives the app stable, intent-named
// methods and one place to grow command-specific behaviour later.
//
// Dedup / coalesce live in the tracker (the underlying FIFO can't iterate), and
// are added only when a command actually needs them.
// -------------------------------------------------------------------

#include <cstddef>

#include <ungula/core/util/queue.h>

namespace ungula::command
{

template <typename CommandT, size_t Capacity> class CommandQueue {
    public:
        /// Enqueue a command for later dispatch. Returns false when full.
        bool submit(const CommandT &cmd) noexcept
        {
                return queue_.push(cmd);
        }

        /// Remove and return the next command. Returns false when empty.
        bool take(CommandT &out) noexcept
        {
                return queue_.pop(out);
        }

        /// Inspect the next command without removing it. Returns false when empty.
        bool peek(CommandT &out) const noexcept
        {
                return queue_.peek(out);
        }

        bool isEmpty() const noexcept
        {
                return queue_.isEmpty();
        }
        bool isFull() const noexcept
        {
                return queue_.isFull();
        }
        size_t count() const noexcept
        {
                return queue_.count();
        }
        constexpr size_t capacity() const noexcept
        {
                return Capacity;
        }
        void clear() noexcept
        {
                queue_.clear();
        }

    private:
        ungula::core::util::Queue<CommandT, Capacity> queue_;
};

} // namespace ungula::command
