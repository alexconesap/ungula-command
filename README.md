# UngulaCommand

Portable command primitives for embedded apps that receive actions from multiple ingress paths (UI, REST, node, cloud, MQTT) and want one consistent command lifecycle.

The library is intentionally small: a shared command vocabulary (`CommandSource`, `CommandResult`, ...), a fixed-size `CommandEnvelope`, a FIFO `CommandQueue`, and an in-flight `CommandTracker` for ACK/timeout handling. No heap, no callbacks, no transport dependency.

If you are wiring `submitCommand(...)` in a project app, this library is the reusable base layer.

## Architecture in one minute

- `CommandEnvelope`: one command unit with source/domain/type/target/flags + optional inline payload (max 16 bytes).
- `CommandQueue<T, N>`: bounded FIFO for commands you cannot dispatch right now.
- `CommandTracker<N>`: bounded table of dispatched-but-not-acked commands.
- `command_types.h`: source/domain/payload/result enums and `toString(...)` helpers.

This split keeps responsibilities clean:

- Queue answers "what is waiting to be dispatched?"
- Tracker answers "what was dispatched and is still waiting for ACK?"

## Quick start

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

namespace {
constexpr uint16_t CMD_START = 1;
constexpr uint16_t CMD_STOP = 2;
}

struct AppCommandBus {
    CommandQueue<CommandEnvelope, 16> queue;
    CommandTracker<16> tracker;
    uint32_t nextId = 1;

    CommandSubmitResult submit(CommandEnvelope cmd, uint32_t nowMs)
    {
        if (cmd.id == 0) {
            cmd.id = nextId++;
        }

        if (!queue.submit(cmd)) {
            return CommandSubmitResult::RejectedQueueFull;
        }

        return CommandSubmitResult::Accepted;
    }
};
```

## Real-world example: small inline payload

Use inline payload for tiny, trivially-copyable params (no heap, no JSON parse in this layer).

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

struct JogStep {
    int16_t delta;
    uint8_t axis;
};

CommandEnvelope buildJogFromUi()
{
    CommandEnvelope cmd;
    cmd.domain = CommandDomain::Project;
    cmd.type = 100; // project command id
    cmd.source = CommandSource::Ui;
    cmd.target = 0; // local

    const JogStep p{.delta = 25, .axis = 1};
    cmd.setInline(p);
    return cmd;
}

bool decodeJog(const CommandEnvelope& cmd, JogStep& out)
{
    return cmd.getInline(out);
}
```

## Real-world example: ACK + timeout flow

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

void onDispatched(CommandTracker<32>& tracker, const CommandEnvelope& cmd, uint32_t nowMs)
{
    (void)tracker.begin(cmd.id, cmd.domain, cmd.type, cmd.source, cmd.target, nowMs);
}

void onAck(CommandTracker<32>& tracker, uint32_t ackedId, CommandResult result)
{
    CommandTracker<32>::Entry entry;
    if (!tracker.complete(ackedId, entry)) {
        return; // late or unknown ACK
    }

    // Route feedback by entry.source, apply result, etc.
    (void)result;
}

void processTimeouts(CommandTracker<32>& tracker, uint32_t nowMs)
{
    constexpr uint32_t kAckTimeoutMs = 1500;
    CommandTracker<32>::Entry expired;
    while (tracker.takeExpired(nowMs, kAckTimeoutMs, expired)) {
        // Treat as CommandResult::Timeout
    }
}
```

## API summary

- `command_types.h`: enums for command source/domain/payload/send/result/submit-result.
- `command_envelope.h`: `CommandEnvelope` + `setInline` / `getInline`.
- `command_queue.h`: `CommandQueue<CommandT, Capacity>`.
- `command_tracker.h`: `CommandTracker<Capacity>` with `begin/complete/takeExpired/hasPending/cancel`.
- `command.h`: umbrella include.

## Constraints

- Inline payload max is `COMMAND_INLINE_PAYLOAD_MAX` (`16` bytes).
- Inline payload type must be trivially copyable.
- `CommandTracker` timeout rule is `nowMs - startedMs >= timeoutMs` (uint32 wrap-safe arithmetic).

## Dependencies

- `UngulaCore` (`ungula::core::util::Queue`)

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation and the tests automation.

## License

MIT - see `LICENSE`.
