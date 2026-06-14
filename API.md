# UngulaCommand

LLM-facing contract for `lib_command`.

Use this API to model command ingress, buffering, dispatch tracking, and ACK timeout handling without coupling to UI, REST, node, or transport implementations.

---

## Include map

| Need | Header |
| --- | --- |
| Everything (types + envelope + queue + tracker) | `#include <ungula/command/command.h>` |
| Shared enums + status strings | `#include <ungula/command/command_types.h>` |
| Command data unit + inline payload helpers | `#include <ungula/command/command_envelope.h>` |
| Bounded FIFO for deferred commands | `#include <ungula/command/command_queue.h>` |
| In-flight dispatched command table | `#include <ungula/command/command_tracker.h>` |

---

## Usage

### Use case: build + decode an inline payload command

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

struct RebootParams {
    uint32_t delayMs;
    bool force;
};

CommandEnvelope cmd;
cmd.domain = CommandDomain::Project;
cmd.type = 42;
cmd.source = CommandSource::Rest;
cmd.target = 0;
cmd.flags = 0;

cmd.setInline(RebootParams{.delayMs = 1000, .force = true});

RebootParams p{};
const bool ok = cmd.getInline(p);
```

When to use this: small typed payloads that fit in 16 bytes and do not justify an external buffer.

### Use case: queue deferred commands

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

CommandQueue<CommandEnvelope, 8> q;
CommandEnvelope cmd;

const bool accepted = q.submit(cmd);

CommandEnvelope next;
if (q.take(next)) {
    // dispatch now
}
```

When to use this: target is busy or dispatcher cadence is slower than ingress cadence.

### Use case: track pending ACKs + detect timeouts

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

CommandTracker<16> tracker;
CommandTracker<16>::Entry out;

(void)tracker.begin(10, CommandDomain::Project, 100, CommandSource::Node, 1, 1000);

// ACK path
const bool resolved = tracker.complete(10, out);

// timeout path
const bool expired = tracker.takeExpired(2600, 1500, out);
```

When to use this: asynchronous dispatch where command result arrives later by ACK or must be synthesized as timeout.

---

## API

### `CommandSource` (`command_types.h`)

Ingress origin:

- `Internal`
- `Ui`
- `Rest`
- `Node`
- `Cloud`
- `Mqtt`

Use this to route feedback (reject/timeout/accept) back to origin policy.

### `CommandDomain` (`command_types.h`)

- `Common`
- `Project`

`type` is interpreted per-domain.

### `CommandPayloadKind` (`command_types.h`)

- `None`
- `InlineBinary`
- `ExternalRawText` (reserved)
- `ExternalBinary` (reserved)

R1 behavior is `None` + `InlineBinary`.

### `SendResult` (`command_types.h`)

Transport-level outcome:

- `Queued`
- `Sent`
- `FailedTransport`

### `CommandResult` (`command_types.h`)

Command-level target verdict:

- `Accepted`
- `RejectedInvalidState`
- `RejectedFaultActive`
- `RejectedBusy`
- `RejectedLimit`
- `RejectedUnsupported`
- `Timeout`

### `CommandSubmitResult` (`command_types.h`)

Immediate answer from `submitCommand`-style ingress:

- `Accepted`
- `RejectedInvalidState`
- `RejectedBusy`
- `RejectedDuplicate`
- `RejectedQueueFull`
- `RejectedUnsupported`

### String helpers (`command_types.h`)

- `const char* toString(CommandResult)`
- `const char* toString(SendResult)`
- `const char* toString(CommandSubmitResult)`

Known values return fixed UPPER_SNAKE_CASE tokens; unknown values return `"UNKNOWN"`.

---

## Public types

### `CommandEnvelope` (`command_envelope.h`)

Fields:

- `uint32_t id` (0 means unassigned)
- `CommandDomain domain`
- `uint16_t type`
- `CommandSource source`
- `uint32_t target` (project-defined)
- `uint32_t flags` (project-defined)
- `CommandPayloadKind payloadKind`
- `uint8_t payloadSize`
- `uint8_t inlinePayload[COMMAND_INLINE_PAYLOAD_MAX]`

Constants:

- `COMMAND_INLINE_PAYLOAD_MAX = 16`

Methods:

- `template <typename T> void setInline(const T& value)`
  - compile-time constraints:
    - `sizeof(T) <= 16`
    - `T` must be trivially copyable
  - sets `payloadKind = InlineBinary`
  - sets `payloadSize = sizeof(T)`

- `template <typename T> bool getInline(T& out) const`
  - returns `false` unless kind is `InlineBinary` and size matches `sizeof(T)`

Contract:

- `CommandEnvelope` is required to be trivially copyable (`static_assert`).

---

## Public functions / methods

### `CommandQueue<CommandT, Capacity>` (`command_queue.h`)

Bounded FIFO facade over `ungula::core::util::Queue`.

- `bool submit(const CommandT& cmd) noexcept`
  - `false` when full

- `bool take(CommandT& out) noexcept`
  - `false` when empty

- `bool peek(CommandT& out) const noexcept`
  - non-destructive read; `false` when empty

- `bool isEmpty() const noexcept`
- `bool isFull() const noexcept`
- `size_t count() const noexcept`
- `constexpr size_t capacity() const noexcept`
- `void clear() noexcept`

### `CommandTracker<Capacity>` (`command_tracker.h`)

Flat fixed-capacity table keyed by `id` for in-flight dispatched commands.

Nested type:

- `struct Entry { id, domain, type, source, target, startedMs, active }`

Methods:

- `bool begin(id, domain, type, source, target, nowMs) noexcept`
  - records one pending command
  - `false` when table full

- `bool complete(id, Entry& out) noexcept`
  - resolves by id and frees slot
  - `false` for unknown/late/duplicate ACK

- `bool takeExpired(nowMs, timeoutMs, Entry& out) noexcept`
  - drains one expired entry per call
  - expiry rule: `(nowMs - startedMs) >= timeoutMs`

- `bool hasPending(domain, type, target) const noexcept`
  - dedup predicate for repeated held commands

- `void cancel(domain, type, target) noexcept`
  - clears matching pending entries without ACK

- `void clear() noexcept`
- `size_t pending() const noexcept`
- `constexpr size_t capacity() const noexcept`

---

## Lifecycle

Typical sequence:

1. Build `CommandEnvelope` from ingress adapter.
2. Optionally assign `id` (if ingress does not).
3. Submit to immediate dispatcher or `CommandQueue`.
4. On dispatch, call `CommandTracker::begin(...)`.
5. On ACK, call `complete(...)`.
6. On periodic tick, drain `takeExpired(...)` and synthesize `CommandResult::Timeout`.

---

## Error handling

- Queue full is explicit (`submit == false` or `RejectedQueueFull`).
- Unknown/late ACK is explicit (`complete == false`).
- Payload decode mismatch is explicit (`getInline == false`).
- Timeout is explicit (`takeExpired == true`, then map to `CommandResult::Timeout`).

No exceptions are used by this library.

---

## Threading / timing / hardware notes

- No ISR locks or atomics inside this library.
- Single-owner access pattern is expected unless caller adds synchronization.
- Timeout arithmetic is `uint32_t` wrap-safe if all time values use the same monotonic clock source.

---

## Internals not part of the public API

- `command_selftest.cpp` is compile smoke only; do not call anything from it.
- `CommandQueue` internals (`ungula::core::util::Queue`) are not part of `lib_command` API contract.

---

## LLM usage rules

- Use `CommandEnvelope` as the single ingress data unit.
- Keep payloads <= 16 bytes inline unless a project-specific external payload pool is introduced.
- Treat `SendResult` and `CommandResult` as distinct layers.
- Use `CommandTracker` for ACK correlation and timeout synthesis; do not invent ad-hoc pending tables.
- Use only documented headers; do not depend on `command_selftest.cpp`.
