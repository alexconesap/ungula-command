// SPDX-License-Identifier: Proprietary
// Copyright (c) 2026 Alex Conesa

#pragma once

// ---------------------------------------------------------------
// CommandErrorRouter — one generic owner of "where did this command come from,
// and where should its error go".
//
// A command can originate at the local UI (touch screen), REST, cloud, MQTT, …
// (CommandSource). When a node ASYNCHRONOUSLY rejects it, the error must reach
// whoever fired it: a UI-sourced rejection shows on the local UI; any REMOTE
// source (Rest/Cloud/Mqtt) is recorded as a "web op error" the remote client
// reads by sequence (so it toasts each one once) — never as a stuck overlay on a
// screen nobody is watching.
//
// The host App composes this: it stamps the source on outgoing messages
// (source()), and on a rejection calls route() with its UI sink. The web-error
// buffer + sequence live here, so a new project inherits the whole mechanism and
// supplies only its UI sink (how to show an error on ITS screen).
// ---------------------------------------------------------------

#include <cstdint>
#include <cstring>

#include "command_types.h" // CommandSource

namespace ungula::command
{

class CommandErrorRouter {
    public:
        /// Source of the command currently being dispatched/sent. The host stamps
        /// this onto the outgoing message so a later rejection can be routed.
        void setSource(CommandSource src)
        {
                source_ = src;
        }
        CommandSource source() const
        {
                return source_;
        }

        /// Route a rejection notice by the command's source. Ui -> uiSink(notice)
        /// (the host shows it on the local screen); any other source -> recorded as
        /// the web op-error. Returns true when a web error was recorded, so the
        /// caller can wake its status broadcast and let the remote client see it.
        template <class UiSink>
        bool route(CommandSource src, const char *notice, UiSink &&uiSink)
        {
                if (src == CommandSource::Ui) {
                        uiSink(notice);
                        return false;
                }
                recordWebError(notice);
                return true;
        }

        /// Record a web-facing op error directly (bumps the sequence). The remote
        /// client toasts it once per new webErrorSeq().
        void recordWebError(const char *notice)
        {
                std::strncpy(webError_, notice != nullptr ? notice : "", sizeof(webError_) - 1);
                webError_[sizeof(webError_) - 1] = '\0';
                ++webErrorSeq_;
        }

        const char *webError() const
        {
                return webError_;
        }
        uint32_t webErrorSeq() const
        {
                return webErrorSeq_;
        }

    private:
        CommandSource source_ = CommandSource::Internal;
        char webError_[64] = {};
        uint32_t webErrorSeq_ = 0;
};

} // namespace ungula::command
