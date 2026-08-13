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
| Gate + domain fan-out + operator notice | `#include <ungula/command/command_ingress.h>` |
| Rejection routing by origin (opt-in) | `#include <ungula/command/command_error_router.h>` |

`command_error_router.h` is NOT pulled in by the umbrella `command.h`. Include it
explicitly when you need it.

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

### Use case: gate every ingress path through one submit

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

struct App {
    CommandIngress<App> ingress;

    // The four hooks CommandIngress calls on the host (duck-typed, no base class):
    bool isProcessRunning() { return running_; }
    bool allowedWhileRunning(const CommandEnvelope& c) { return c.type == CMD_ABORT; }
    CommandSubmitResult dispatchCommon(const CommandEnvelope&)  { return CommandSubmitResult::Accepted; }
    CommandSubmitResult dispatchProject(const CommandEnvelope&) { return CommandSubmitResult::Accepted; }

    CommandSubmitResult submitCommand(CommandEnvelope cmd)
    {
        const CommandSubmitResult r = ingress.submit(*this, cmd);
        if (r == CommandSubmitResult::RejectedBusy) {
            ingress.stageNotice("Busy - process running");
        }
        return r;
    }

    bool running_ = false;
};
```

When to use this: the project has more than one input path (UI, REST, node,
cloud) and wants one run-state gate covering all of them.

### Use case: gate now, dispatch later (queued ingress)

```cpp
// On the ingress thread: gate only, then queue.
CommandEnvelope cmd = buildFromRest();
if (app.ingress.gate(app, cmd) != CommandSubmitResult::Accepted) {
    return; // refused at the gate; cmd.id is already assigned
}
myQueue.push(cmd);

// Later, on the main loop, drain and execute:
CommandEnvelope next;
while (myQueue.pop(next)) {
    (void)app.ingress.dispatch(app, next);
}
```

When to use this: the ingress runs on a task/callback that must not execute the
command in place. `gate()` takes `CommandEnvelope&` and assigns `id` in place, so
read `cmd.id` after the call to correlate the later ACK. `lib_command` does not
supply the queue — the project owns it.

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

### `CommandIngress<class Host, uint16_t NoticeCapacity = 64>` (`command_ingress.h`)

Header-only template. No heap, no virtuals. `Host` is duck-typed: it is only
required to provide the four members below, resolved at instantiation.

Required on `Host`:

| Member | Used by |
| --- | --- |
| `bool isProcessRunning()` | `gate()` |
| `bool allowedWhileRunning(const CommandEnvelope&)` | `gate()` |
| `CommandSubmitResult dispatchCommon(const CommandEnvelope&)` | `dispatch()` |
| `CommandSubmitResult dispatchProject(const CommandEnvelope&)` | `dispatch()` |

Methods:

- `CommandSubmitResult gate(Host& host, CommandEnvelope& cmd)`
  - Assigns `cmd.id = ++counter_` **only when `cmd.id == 0`**. A caller that
    pre-sets a non-zero `id` keeps it; the ingress does not check it for
    uniqueness.
  - Returns `RejectedBusy` when `host.isProcessRunning()` is true and
    `host.allowedWhileRunning(cmd)` is false. Otherwise `Accepted`.
  - Does NOT execute the command.
  - `counter_` is a plain `uint32_t` and wraps to 0 after 2^32-1 submissions;
    the id after the wrap is 1, not 0.

- `CommandSubmitResult dispatch(Host& host, const CommandEnvelope& cmd)`
  - `CommandDomain::Common` -> `host.dispatchCommon(cmd)`;
    `CommandDomain::Project` -> `host.dispatchProject(cmd)`.
  - Returns `RejectedUnsupported` only for a `domain` value outside the enum.
  - Does not re-run the gate. Do not call it on an un-gated command.

- `CommandSubmitResult submit(Host& host, const CommandEnvelope& cmdIn)`
  - `gate()` then `dispatch()`. Takes `cmdIn` **by const reference and copies
    it**, so the assigned `id` is NOT visible to the caller. Use `gate()` +
    `dispatch()` when you need the id.

- `void stageNotice(const char* text)`
  - Copies into a fixed `char[NoticeCapacity]` buffer, truncating at
    `NoticeCapacity - 1`. `nullptr` is ignored. Overwrites any notice not yet
    drained — only the newest survives.

- `bool takeNotice(char* buf, size_t len)`
  - Copies the pending notice into `buf` (truncated to `len - 1` plus a
    terminator) and clears the pending flag. Returns `false` when nothing is
    pending, `buf` is `nullptr`, or `len == 0`.

Storage: one `uint32_t`, one `char[NoticeCapacity]`, one `bool`. Default
capacity 64 bytes.

### `CommandErrorRouter` (`command_error_router.h`)

Non-template class that answers "a command was rejected — where does the error
go". Not included by `command.h`; include the header directly.

- `void setSource(CommandSource src)` / `CommandSource source() const`
  - Stamp the origin of the command being dispatched so a later async rejection
    can be routed. Defaults to `CommandSource::Internal`.

- `template <class UiSink> bool route(CommandSource src, const char* notice, UiSink&& uiSink)`
  - `src == CommandSource::Ui` -> calls `uiSink(notice)` and returns `false`.
  - Any other source -> `recordWebError(notice)` and returns `true`, so the
    caller knows to wake its status broadcast.
  - Note it routes on the `src` argument passed in, not on the stored `source()`.

- `void recordWebError(const char* notice)`
  - Copies into a fixed `char[64]` (truncating at 63); `nullptr` stores `""`.
    Increments the sequence counter.

- `const char* webError() const` — pointer to the internal buffer. Valid until
  the next `recordWebError()`/`route()`; copy it if you need to keep it.
- `uint32_t webErrorSeq() const` — bumped on every recorded web error. Remote
  clients poll it and toast once per new value. Starts at 0, so 0 means "no
  error recorded yet".

Only the newest web error is retained — there is no queue.

---

## Lifecycle

Typical sequence:

1. Build a `CommandEnvelope` in the ingress adapter (set `domain`, `type`,
   `source`, and any inline payload). Leave `id` at 0.
2. `gate()` assigns the id and applies the run-state gate.
3. `dispatch()` fans out to the host's domain handler — immediately (via
   `submit()`) or later from the loop.
4. On rejection, `stageNotice()` for the local UI, or `CommandErrorRouter` when
   the origin may be remote.

`CommandIngress` holds no state that needs construction or teardown: default
construct it as a member and use it. There is no `begin()`/`end()`.

---

## Error handling

- Submit rejection is explicit — `gate()`/`dispatch()`/`submit()` all return a
  `CommandSubmitResult`. Every one of them is `[[nodiscard]]`-worthy but not
  marked as such; do not ignore the return.
- `gate()` produces only `Accepted` or `RejectedBusy`. The other
  `CommandSubmitResult` values (`RejectedDuplicate`, `RejectedQueueFull`,
  `RejectedInvalidState`, `RejectedUnsupported`) are for the host's own handlers
  to return — this library never generates them, except `RejectedUnsupported`
  from `dispatch()` on an out-of-range `domain`.
- Payload decode mismatch is explicit (`getInline() == false`).
- `takeNotice()` returning `false` means "nothing staged", not an error.

No exceptions are used by this library. Nothing here allocates.

---

## Threading / timing / hardware notes

- No locks, no atomics, no ISR guards anywhere in this library.
- `CommandIngress` is single-owner. `gate()` mutates the id counter and
  `stageNotice()`/`takeNotice()` mutate the notice buffer without protection —
  calling them from two tasks races. Serialize at the host, or keep one ingress
  per task.
- This library has no notion of time: no clock, no timeouts, no timers.
  `CommandResult::Timeout` is a value the host synthesises itself when its own
  ACK deadline passes; `lib_command` never produces it.

---

## Internals not part of the public API

- `command_selftest.cpp` is compile smoke only; do not call anything from it.
- `CommandIngress::counter_`, `notice_`, `noticePending_` and
  `CommandErrorRouter::webError_`, `webErrorSeq_` are private storage.
- `lib_command` declares `depends=UngulaCore` in `library.properties`, but no
  header here includes anything from UngulaCore today. Do not rely on core
  symbols being transitively available through this library.

---

## LLM usage rules

- Use `CommandEnvelope` as the single ingress data unit.
- Keep payloads <= 16 bytes inline unless a project-specific external payload pool is introduced.
- Use only documented headers; do not depend on `command_selftest.cpp`.
- `CommandIngress` does not queue. If the task needs queuing, dedup, or ACK
  tracking, that is the host's code — say so rather than assuming this library
  provides it.
- Do not add a `default:` label when switching over `CommandDomain` /
  `CommandResult` / `CommandSubmitResult` in new code unless you also handle
  future values; the enums are versioned wire-ish vocabulary.
