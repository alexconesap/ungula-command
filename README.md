# UngulaCommand

Portable command primitives for embedded apps that receive actions from multiple ingress paths (UI, REST, node, cloud, MQTT) and want one consistent command lifecycle.

The library is intentionally small: a shared command vocabulary (`CommandSource`, `CommandResult`, ...), a fixed-size `CommandEnvelope`, and a `CommandIngress` skeleton (correlation-id counter, run-state gate, domain fan-out, one-shot operator notice). No heap, no callbacks, no transport dependency.

If you are wiring `submitCommand(...)` in a project app, this library is the reusable base layer.

## Architecture in one minute

- `CommandEnvelope`: one command unit with source/domain/type/target/flags + optional inline payload (max 16 bytes).
- `CommandIngress<Host>`: the single authoritative submit path — assigns a correlation id, gates by run-state, fans out to the Common/Project domain handler, and stages a one-shot operator notice for the UI.
- `command_types.h`: source/domain/payload/result enums and `toString(...)` helpers.

The machine-specific parts (`isProcessRunning`, `allowedWhileRunning`, `dispatchCommon`, `dispatchProject`) are injected through the host, duck-typed at instantiation — no virtuals, no heap.

## Quick start

```cpp
#include <ungula/command/command.h>

using namespace ungula::command;

namespace {
constexpr uint16_t CMD_START = 1;
constexpr uint16_t CMD_STOP = 2;
}

struct App {
    CommandIngress<App> ingress;

    // Host hooks the ingress calls into:
    bool isProcessRunning() { return false; }
    bool allowedWhileRunning(const CommandEnvelope&) { return true; }
    CommandSubmitResult dispatchCommon(const CommandEnvelope&) { return CommandSubmitResult::Accepted; }
    CommandSubmitResult dispatchProject(const CommandEnvelope&) { return CommandSubmitResult::Accepted; }

    CommandSubmitResult submit(CommandEnvelope cmd)
    {
        return ingress.submit(*this, cmd);
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

## API summary

- `command_types.h`: enums for command source/domain/payload/result/submit-result.
- `command_envelope.h`: `CommandEnvelope` + `setInline` / `getInline`.
- `command_ingress.h`: `CommandIngress<Host>` with `gate/dispatch/submit/stageNotice/takeNotice`.
- `command.h`: umbrella include.

## Constraints

- Inline payload max is `COMMAND_INLINE_PAYLOAD_MAX` (`16` bytes).
- Inline payload type must be trivially copyable.

## Dependencies

- `UngulaCore` (`ungula::core::util::Queue`)

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation and the tests automation.

## License

MIT - see `LICENSE`.
