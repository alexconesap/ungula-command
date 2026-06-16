// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// CommandIngress — the project-agnostic ingress skeleton behind a single
// authoritative submitCommand(). Owns the parts that are identical on every
// machine:
//   * the correlation-id counter (assigns an id to commands that arrive without
//     one),
//   * the run-state gate (while the host reports a process running, only the
//     host-approved commands pass — the rest are refused Busy),
//   * the domain fan-out (Common -> framework handler, Project -> project
//     handler),
//   * the one-shot operator notice a rejected command stages for the UI to drain.
//
// The machine-specific parts are injected through the Host (duck-typed, resolved
// at instantiation — no virtuals, no heap, and no lib_node / UI / transport
// dependency reaches this header):
//   bool                 isProcessRunning();
//   bool                 allowedWhileRunning(const CommandEnvelope&);
//   CommandSubmitResult  dispatchCommon(const CommandEnvelope&);
//   CommandSubmitResult  dispatchProject(const CommandEnvelope&);
//
// A project embeds CommandIngress<App> and routes submitCommand() through it;
// the gate then covers every input path (UI / REST / cloud / node) from one spot.
// -------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ungula/command/command_envelope.h"
#include "ungula/command/command_types.h"

namespace ungula::command
{

template <class Host, uint16_t NoticeCapacity = 64> class CommandIngress
{
public:
        /// Submit-time gate. Assigns a correlation id (if unset) and runs the
        /// run-state gate. Returns Accepted when the command may proceed (be
        /// queued or dispatched), or a Rejected* reason. Does NOT execute — split
        /// from dispatch() so an async ingress can gate now and dispatch later
        /// (from the loop), while a synchronous caller still uses submit().
        CommandSubmitResult gate(Host &host, CommandEnvelope &cmd)
        {
                if (cmd.id == 0) {
                        cmd.id = ++counter_;
                }
                // Single run-state gate: while a process runs, only the commands
                // the host approves get through; everything else is refused Busy.
                if (host.isProcessRunning() && !host.allowedWhileRunning(cmd)) {
                        return CommandSubmitResult::RejectedBusy;
                }
                return CommandSubmitResult::Accepted;
        }

        /// Fan an already-gated command out to its domain handler — the actual
        /// execution + per-command verdict. An async ingress calls this from the
        /// loop as it drains the queue.
        CommandSubmitResult dispatch(Host &host, const CommandEnvelope &cmd)
        {
                switch (cmd.domain) {
                case CommandDomain::Common:
                        return host.dispatchCommon(cmd);
                case CommandDomain::Project:
                        return host.dispatchProject(cmd);
                }
                return CommandSubmitResult::RejectedUnsupported;
        }

        /// Synchronous gate + dispatch (the original behaviour). Kept for callers
        /// that execute in place rather than queue.
        CommandSubmitResult submit(Host &host, const CommandEnvelope &cmdIn)
        {
                CommandEnvelope cmd = cmdIn;
                const CommandSubmitResult gated = gate(host, cmd);
                if (gated != CommandSubmitResult::Accepted) {
                        return gated;
                }
                return dispatch(host, cmd);
        }

        /// One-shot operator notice: a rejected command or a validation failure
        /// stages a short message; the UI drains it once. Fixed buffer, no heap.
        void stageNotice(const char *text)
        {
                if (text == nullptr) {
                        return;
                }
                std::strncpy(notice_, text, sizeof(notice_) - 1);
                notice_[sizeof(notice_) - 1] = '\0';
                noticePending_ = true;
        }

        /// Drain the staged notice into buf (truncated to len). Returns false when
        /// nothing is pending.
        bool takeNotice(char *buf, size_t len)
        {
                if (!noticePending_ || buf == nullptr || len == 0) {
                        return false;
                }
                std::strncpy(buf, notice_, len - 1);
                buf[len - 1] = '\0';
                noticePending_ = false;
                return true;
        }

private:
        uint32_t counter_ = 0; // monotonic correlation-id source
        char notice_[NoticeCapacity] = {};
        bool noticePending_ = false;
};

} // namespace ungula::command
