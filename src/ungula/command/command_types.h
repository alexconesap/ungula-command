// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

// -------------------------------------------------------------------
// lib_command — project-agnostic application command primitives.
//
// One authoritative ingress (App::submitCommand) for every input path (UI,
// REST, node, cloud, MQTT). This header holds the shared vocabulary: where a
// command came from, which half of the type-space owns it, what kind of payload
// it carries, and the two DISTINCT outcomes — transport-level vs command-level.
//
// No heap, no std::function, no virtuals, no dependency on lib_node / UI / any
// transport. A project layers its own command enums (per domain) + dispatchers
// on top.
// -------------------------------------------------------------------

#include <cstdint>

namespace ungula::command
{

/// Where a command entered the system. The app treats all sources the same;
/// this is for audit, policy, and routing rejection feedback to the right place.
enum class CommandSource : uint8_t {
        Internal = 0, // app logic / FSM
        Ui = 1, // embedded UI (touch)
        Rest = 2, // HTTP REST API
        Node = 3, // inbound from a peer node
        Cloud = 4, // cloud poll / WebSocket
        Mqtt = 5, // MQTT ingress (Rachel-style)
};

/// Splits the type-space: the framework owns Common command handling, the
/// project owns Project command handling. `type` is interpreted per domain.
enum class CommandDomain : uint8_t {
        Common = 0,
        Project = 1,
};

/// Payload strategy. R1 implements None + InlineBinary; the external variants
/// are reserved so a future project (e.g. raw MQTT JSON kept in a fixed-capacity
/// pool) can extend WITHOUT changing the envelope shape.
enum class CommandPayloadKind : uint8_t {
        None = 0,
        InlineBinary = 1,
        ExternalRawText = 2, // reserved — deferred
        ExternalBinary = 3, // reserved — deferred
};

/// Command-level outcome — the target's verdict on the requested operation.
/// Arrives via the ACK path: immediately for synchronous transports, later for
/// asynchronous ones. Both invoke the same onCommandAck() handling.
enum class CommandResult : uint8_t {
        Accepted = 0,
        RejectedInvalidState = 1,
        RejectedFaultActive = 2,
        RejectedBusy = 3,
        RejectedLimit = 4,
        RejectedUnsupported = 5,
        Timeout = 6, // no ACK arrived in time (synthesised MAIN-side)
};

/// Synchronous answer to App::submitCommand — "was this command admitted". An
/// admitted command may still be rejected later by the target (that arrives as a
/// CommandResult via the ACK path).
enum class CommandSubmitResult : uint8_t {
        Accepted = 0, // executed now, or queued, or sent to a node
        RejectedInvalidState = 1,
        RejectedBusy = 2,
        RejectedDuplicate = 3, // an equivalent command is already pending
        RejectedQueueFull = 4,
        RejectedUnsupported = 5,
};

inline const char *toString(CommandResult r)
{
        switch (r) {
        case CommandResult::Accepted:
                return "ACCEPTED";
        case CommandResult::RejectedInvalidState:
                return "REJECTED_INVALID_STATE";
        case CommandResult::RejectedFaultActive:
                return "REJECTED_FAULT_ACTIVE";
        case CommandResult::RejectedBusy:
                return "REJECTED_BUSY";
        case CommandResult::RejectedLimit:
                return "REJECTED_LIMIT";
        case CommandResult::RejectedUnsupported:
                return "REJECTED_UNSUPPORTED";
        case CommandResult::Timeout:
                return "TIMEOUT";
        default:
                return "UNKNOWN";
        }
}

inline const char *toString(CommandSubmitResult r)
{
        switch (r) {
        case CommandSubmitResult::Accepted:
                return "ACCEPTED";
        case CommandSubmitResult::RejectedInvalidState:
                return "REJECTED_INVALID_STATE";
        case CommandSubmitResult::RejectedBusy:
                return "REJECTED_BUSY";
        case CommandSubmitResult::RejectedDuplicate:
                return "REJECTED_DUPLICATE";
        case CommandSubmitResult::RejectedQueueFull:
                return "REJECTED_QUEUE_FULL";
        case CommandSubmitResult::RejectedUnsupported:
                return "REJECTED_UNSUPPORTED";
        default:
                return "UNKNOWN";
        }
}

} // namespace ungula::command
