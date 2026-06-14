// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// CommandEnvelope — the project-agnostic application command an ingress adapter
// builds and hands to App::submitCommand(). The (domain, type) pair selects the
// handler; `id` correlates the asynchronous ACK back to this command. Small
// typed params ride inline (no heap); large/raw payloads will use an external
// reference (reserved — see CommandPayloadKind).
//
// Plain copyable struct: no virtuals, no heap, trivially movable through the
// fixed-storage queue/tracker.
// -------------------------------------------------------------------

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "ungula/command/command_types.h"

namespace ungula::command
{

/// Bytes carried directly in the envelope (no external storage). Small typed
/// params (reboot delay, jog direction, tension step) fit here.
constexpr uint8_t COMMAND_INLINE_PAYLOAD_MAX = 16;

struct CommandEnvelope {
        uint32_t id = 0; // correlation id (0 = unassigned; submit assigns)
        CommandDomain domain = CommandDomain::Common;
        uint16_t type = 0; // command type within the domain
        CommandSource source = CommandSource::Internal;
        uint32_t target = 0; // project-defined (e.g. node index / ALL); 0 = local
        uint32_t flags = 0; // project-defined modifier bits

        CommandPayloadKind payloadKind = CommandPayloadKind::None;
        uint8_t payloadSize = 0; // bytes used in inlinePayload
        uint8_t inlinePayload[COMMAND_INLINE_PAYLOAD_MAX] = {};

        /// Stash a small trivially-copyable params struct inline. Compile-time
        /// checked to fit; no heap.
        template <typename T> void setInline(const T &value)
        {
                static_assert(sizeof(T) <= COMMAND_INLINE_PAYLOAD_MAX, "payload too large for inline storage");
                static_assert(std::is_trivially_copyable<T>::value, "inline payload must be trivially copyable");
                std::memcpy(inlinePayload, &value, sizeof(T));
                payloadSize = static_cast<uint8_t>(sizeof(T));
                payloadKind = CommandPayloadKind::InlineBinary;
        }

        /// Read back the inline params. Returns false if the kind/size doesn't
        /// match T (wrong/absent payload).
        template <typename T> bool getInline(T &out) const
        {
                static_assert(std::is_trivially_copyable<T>::value, "inline payload must be trivially copyable");
                if (payloadKind != CommandPayloadKind::InlineBinary || payloadSize != sizeof(T)) {
                        return false;
                }
                std::memcpy(&out, inlinePayload, sizeof(T));
                return true;
        }
};

static_assert(std::is_trivially_copyable<CommandEnvelope>::value,
              "CommandEnvelope must be trivially copyable (queue/tracker storage)");

} // namespace ungula::command
