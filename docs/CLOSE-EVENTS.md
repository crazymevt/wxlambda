# Close-event family and veto

This C1 slice extends bindings beyond command events with `:close`, backed by
`wxEVT_CLOSE_WINDOW` and `wxCloseEvent`. The four command kinds remain supported.
`event-family` classifies these snapshots as `:command` or `:close`;
registered [notify events](NOTIFY-EVENTS.md) additionally use `:notify`; this is a portable
supported-kind classification, not arbitrary native RTTI or event registration.
The baseline is wxWidgets 3.2.12 and the native bridge is ABI 19.

The native type lookup is now **`event-type`**, replacing `command-event-type`
without an alias. It accepts the seven built-in kinds and registered command/notify names. Native type IDs remain
process-local; saved snapshots keep immutable copied values.

## Veto a real frame close request

```lisp
(wxlambda:bind-event frame :close
  (lambda (snapshot context)
    (format t "Close can be vetoed: ~S~%" (wxlambda:event-can-veto-p snapshot))
    (if (wxlambda:event-can-veto-p context)
        (setf (wxlambda:event-vetoed-p context) t)
        (setf (wxlambda:event-skipped-p context) t)))
  :with-context t)
(wxlambda:close-window frame) ; NIL if that handler vetoes
```

`event-can-veto-p` reads the snapshot's captured capability or the current native
context's `CanVeto`. Command snapshots/contexts report NIL. `event-vetoed-p` reads
the snapshot's captured flag or the current close context's `GetVeto`. Snapshots
capture state **before this callback** and never reflect later context mutation.
Saved flags can be read without a native session; snapshots have no SETF method.

The canonical generic `(setf event-vetoed-p)` writer applies only to the current
close context. It checks the actual native event class and `CanVeto` before
calling `Veto(bool)`. Setting or clearing a non-vetoable close, accessing live veto
state on a command context, or using an expired/suspended context signals
`native-error` before any wx assertion or invalid pointer access. The writer
returns the supplied generalized boolean. Vetoable contexts permit both veto
and clearing a veto. Context SETF `event-can-veto-p` now maps SetCanVeto
directly, including false to true. It changes metadata and can hide/restore
a stored veto; see [metadata semantics](EVENT-METADATA.md).

## Acceptance, consumption, veto and destruction

`close-window` is a generic frame-only operation that synchronously calls
`wxWindow::Close(force)`. It preserves the native acceptance boolean; it does
not promise destruction. A contextual callback begins unskipped and may consume
the request without deleting anything. An accepted request can therefore return
T while the frame remains alive. Delegate with Skip true when native default
frame closing should run. Use `destroy-window` to request explicit destruction.

A vetoing contextual handler should consume the close event (leave Skip false),
so native default closing is not run. Veto and Skip are independent: setting veto
is not a substitute for controlling which later native handlers execute. Veto
also cannot undo an independent `destroy-window` request.

`:force t` sets native `CanVeto` false. Both true and NIL veto assignments then
signal `native-error` while CanVeto stays false. A handler can explicitly
change CanVeto through SETF, affecting native closing results. Force does not bypass a contextual
handler's consumption or guarantee deletion. A handler should check capability
and choose the appropriate native default/explicit closing action.

Observer bindings still automatically Skip. **A callback error does not itself
veto** an observer-delivered close: native default handling may continue before
the condition is re-signaled. Use contextual policy and explicit veto when
rejection is required. Callback errors from `close-window` have a nearer Lisp
boundary, like notifying writers; handling a nested close error does not stop
an enclosing RUN. Errors escaping an actual native UI callback use the existing
callback error/loop-stop rules.

## Synthetic close events

```lisp
(wxlambda:queue-event receiver :close :id 42)                 ; vetoable
(wxlambda:queue-event receiver :close :id 42 :can-veto-p nil) ; not vetoable
```

The queue makes an owned copy of a `wxCloseEvent`, rather than incorrectly
treating it as `wxCommandEvent`. `:can-veto-p` defaults T and is accepted only for
`:close`; it sets initial capability, not an existing context's native flag.
Close events reject non-default text/checked/selection payloads. They do not
normally propagate to parents as command events do, although base event context
Skip/propagation operations remain available.

A synthetic close queued to a frame **may invoke its native default closing
action**. A standalone handler has no frame default. Queuing provides no
synchronous acceptance result and is not a substitute for `close-window` when
that result is needed. Session/main-thread, ID filter, weak lifetime, safe
unbinding and deferred callback-disposal rules remain as documented in
[bindings](EVENT-BINDINGS.md) and [scoped contexts](EVENT-CONTEXTS.md).

## Scope still missing

This guide covers `wxEVT_CLOSE_WINDOW`; [session events](SESSION-EVENTS.md)
add query/end-session tags and logging-off state. Registered command/notify
types are supported. Native control-specific notify subclasses, arbitrary
native event classes/factories,
persistent event ownership/cloning, origin/ID/type mutation and userData remain
open. The frame-only close request is a bounded mapping of an inherited window
operation; panel/dialog/other top-level receivers are not exposed here. Native
virtual callbacks, pushed handler ownership, timer/idle and worker dispatch also
remain C1 work. No claim of full wxCloseEvent, wxEvent or C1 coverage is made.

## Example and checks

The [close-events example](../examples/close-events.lisp) blocks frame closing
until a checkbox permits it. Its callback releases the button's control-specific
closure before delegating to default closing. The reusable `exercise` runs real
close requests through the button callback and checks veto, liveness and cleanup:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/close-events.lisp \
  --eval '(wxlambda-close-events:start)'
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/close-events.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/close-events.lisp
python3 tools/coverage_inventory.py /path/to/wxWidgets-3.2.12/interface
python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. Focused tests cover real
close acceptance versus deletion, forced requests, immutable flags, synthetic
events, wrong-family rejection, expired contexts and handled nested close errors
while RUN continues. These APIs, this guide and example are included in the
[Cadre docset](DOCSET.md), installable under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`). Hosted GUI and graphical Cadre checks
remain separate from local shared-reader validation.

[Session events](SESSION-EVENTS.md) extend the close family with query/end-session kinds, scoped logging-off metadata and isolated synthetic dispatch. Native OS session delivery remains unverified.
