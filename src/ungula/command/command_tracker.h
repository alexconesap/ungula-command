// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// CommandTracker — fixed-capacity table of commands that were DISPATCHED to a
// target and are awaiting their ACK. Generalises the ad-hoc "pending acks"
// arrays into one reusable, transport-agnostic primitive:
//
//   begin(id, …)        when a command is dispatched
//   complete(id, out)   when its ACK arrives (any transport, sync or async)
//   takeExpired(…)      drains commands whose ACK never came (-> Timeout)
//   hasPending(…)       true if an equivalent command is still in flight (dedup,
//                       e.g. drop a held T+ while one is unacked)
//
// No heap, no std::function, no virtuals. Random-access by id, so it's a flat
// array with active flags rather than a FIFO.
// -------------------------------------------------------------------

#include <cstddef>
#include <cstdint>

#include "ungula/command/command_types.h"

namespace ungula::command
{

template <size_t Capacity> class CommandTracker {
    public:
        /// One dispatched-but-unacked command. `target`/`type` are project-defined;
        /// `source` lets the app route rejection/timeout feedback to its origin.
        struct Entry {
                uint32_t id = 0;
                CommandDomain domain = CommandDomain::Common;
                uint16_t type = 0;
                CommandSource source = CommandSource::Internal;
                uint32_t target = 0;
                uint32_t startedMs = 0;
                bool active = false;
        };

        /// Record a dispatched command awaiting its ACK. Returns false if full.
        bool begin(uint32_t id, CommandDomain domain, uint16_t type, CommandSource source, uint32_t target,
                   uint32_t nowMs) noexcept
        {
                Entry *slot = freeSlot();
                if (slot == nullptr) {
                        return false;
                }
                slot->id = id;
                slot->domain = domain;
                slot->type = type;
                slot->source = source;
                slot->target = target;
                slot->startedMs = nowMs;
                slot->active = true;
                return true;
        }

        /// Resolve a pending command by id (its ACK arrived). Copies the tracked
        /// entry into `out` and frees the slot. Returns false when no such pending
        /// id exists (a late, duplicate, or unknown ACK — caller can ignore it).
        bool complete(uint32_t id, Entry &out) noexcept
        {
                for (Entry &e : entries_) {
                        if (e.active && e.id == id) {
                                out = e;
                                e.active = false;
                                return true;
                        }
                }
                return false;
        }

        /// Drain one command whose ACK is overdue (now - startedMs >= timeoutMs).
        /// Call repeatedly each tick until it returns false; treat each drained
        /// entry as CommandResult::Timeout.
        bool takeExpired(uint32_t nowMs, uint32_t timeoutMs, Entry &out) noexcept
        {
                for (Entry &e : entries_) {
                        if (e.active && (nowMs - e.startedMs) >= timeoutMs) {
                                out = e;
                                e.active = false;
                                return true;
                        }
                }
                return false;
        }

        /// True if a command with this (domain, type, target) is still in flight.
        /// Used to drop duplicates of a held/repeated command before re-sending.
        bool hasPending(CommandDomain domain, uint16_t type, uint32_t target) const noexcept
        {
                for (const Entry &e : entries_) {
                        if (e.active && e.domain == domain && e.type == type && e.target == target) {
                                return true;
                        }
                }
                return false;
        }

        /// Forget all pending commands of a given (type, target) without acking —
        /// e.g. cancel a held command after one rejection so it stops repeating.
        void cancel(CommandDomain domain, uint16_t type, uint32_t target) noexcept
        {
                for (Entry &e : entries_) {
                        if (e.active && e.domain == domain && e.type == type && e.target == target) {
                                e.active = false;
                        }
                }
        }

        void clear() noexcept
        {
                for (Entry &e : entries_) {
                        e.active = false;
                }
        }

        size_t pending() const noexcept
        {
                size_t n = 0;
                for (const Entry &e : entries_) {
                        if (e.active) {
                                ++n;
                        }
                }
                return n;
        }

        constexpr size_t capacity() const noexcept
        {
                return Capacity;
        }

    private:
        Entry *freeSlot() noexcept
        {
                for (Entry &e : entries_) {
                        if (!e.active) {
                                return &e;
                        }
                }
                return nullptr;
        }

        Entry entries_[Capacity];
};

} // namespace ungula::command
