# UngulaCommand

LLM-facing contract for `lib_command`.

Use this API to model command ingress without coupling to UI, REST, node, or transport implementations.

---

## Include map

| Need | Header |
| --- | --- |
| Everything (types + envelope + ingress) | `#include <ungula/command/command.h>` |
| Shared enums + status strings | `#include <ungula/command/command_types.h>` |
| Command data unit + inline payload helpers | `#include <ungula/command/command_envelope.h>` |

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

## Lifecycle

Typical sequence:

1. Build `CommandEnvelope` from ingress adapter.
2. Optionally assign `id` (if ingress does not).
3. Submit to the project's dispatcher.

---

## Error handling

- Submit rejection is explicit (`RejectedQueueFull` / other `CommandSubmitResult`).
- Payload decode mismatch is explicit (`getInline == false`).

No exceptions are used by this library.

---

## Threading / timing / hardware notes

- No ISR locks or atomics inside this library.
- Single-owner access pattern is expected unless caller adds synchronization.
- Timeout arithmetic is `uint32_t` wrap-safe if all time values use the same monotonic clock source.

---

## Internals not part of the public API

- `command_selftest.cpp` is compile smoke only; do not call anything from it.
- `CommandIngress` internals (`ungula::core::util::Queue`) are not part of `lib_command` API contract.

---

## LLM usage rules

- Use `CommandEnvelope` as the single ingress data unit.
- Keep payloads <= 16 bytes inline unless a project-specific external payload pool is introduced.
- Use only documented headers; do not depend on `command_selftest.cpp`.
