# Event-handler ownership and lifetime

This C1 foundation slice introduces `event-handler`, a native-backed CLOS wrapper
for `wxEvtHandler` and the superclass of `window`. It establishes ownership and
state before generic binding/unbinding, timers and broader event routing.
The coverage baseline is wxWidgets 3.2.12; current native ABI is 16.
Four command-event observer kinds now have [independent bindings](EVENT-BINDINGS.md).

## Owned standalone handlers

```lisp
(defclass named-handler (wxlambda:event-handler)
  ((name :initarg :name :reader name))
  (:default-initargs :name "worker-results"))

(defvar handler (wxlambda:make-event-handler :class 'named-handler))
(wxlambda:handler-alive-p handler)           ; T
(setf (wxlambda:handler-enabled-p handler) nil)
(wxlambda:handler-enabled-p handler)         ; NIL
(wxlambda:destroy-event-handler handler)     ; T
(wxlambda:handler-alive-p handler)           ; NIL in this active session
```

Use these forms inside `with-application`. Direct `make-instance` is supported;
subclass slot initargs, defaults and AFTER hooks work normally. A subclass AFTER
hook can read native state. No native initargs are currently exposed on the
standalone base. The thin keyword-only factory forwards subclass initargs and
accepts a class symbol or class object, rejecting window and non-handler classes.

The session owns each standalone native handler. Explicit destruction deletes it
immediately outside callbacks. During callbacks the target is invalidated and
native deletion is deferred until its dispatch/setter stack returns; any remaining standalone handlers are deleted during session cleanup,
before native application exit. Dropping a Lisp wrapper does not delete its native
object: no GC finalizer is installed. Failed managed initialization rolls back
native handlers/windows allocated in that initialization and releases callback
roots, preserving the original condition or nonlocal exit.

Repeated explicit destruction is an error. Reads/writes on destroyed handlers
signal `native-error`, except `handler-alive-p`, which returns NIL for a stale
native identity during an active session. Failed/uninitialized wrappers have no
handle and signal `native-error`. All three operations require an active session;
after session cleanup even the liveness query signals `native-error`.

## Borrowed window identities

Every concrete window inherits `event-handler`. It uses its existing `wxWindow`
as the native `wxEvtHandler` base, allocating **one native widget**, with no second
standalone handler. Existing window construction, subclass defaults and AFTER
hooks remain unchanged. The window is still owned according to its window/parent
lifetime. `destroy-event-handler` rejects window wrappers; use `destroy-window`
and the existing parent-ownership rules. The raw bridge enforces this distinction.

This interface refers to the window's own native base identity. It does not wrap
an arbitrary `GetEventHandler()` chain, push/pop handlers, transfer ownership or
allow supplied native handles. The base `window` class remains abstract.

## Event processing versus window input

```lisp
(assert (typep button 'wxlambda:event-handler))
(setf (wxlambda:handler-enabled-p button) nil)
(wxlambda:handler-enabled-p button) ; NIL: its handler skips processing
(wxlambda:enabled-p button)         ; window input state is unchanged
```

`handler-enabled-p` reads `wxEvtHandler::GetEvtHandlerEnabled`; its canonical
generic SETF writer calls `SetEvtHandlerEnabled`, normalizes a generalized boolean
and returns the supplied value. This is the own event-processing flag, separate
from `enabled-p` and ancestor input state. It does not grey out a control, synthesize
a command event or promise to suppress processing elsewhere in a native handler
chain or parent propagation route. Native wxWidgets routing rules still apply.

Disabling event processing can suppress `wxEVT_DESTROY` delivery. Native window
lookup therefore also uses weak tracking, independent of that event, before
dereferencing a pointer. Destruction invalidates the window and child identities
even if their handlers are disabled. The existing native destroy hook removes
ordinary registry entries; dead weak entries may remain until session
cleanup but cannot be dereferenced. This is internal tracking, not a public
`wxWeakRef` wrapper.

All operations are restricted to the main-thread GUI session. Callback roots
for the existing control-specific handlers still require explicit unbinding
before window destruction, or they remain rooted until session cleanup.
Standalone handlers, windows and the borrowed application view support built-in and registered command/notify observer kinds,
ID filters, safe generic unbinding and copied synthetic queues; see
[command-event bindings](EVENT-BINDINGS.md). Arbitrary event types, pushed/linked
handler ownership, filters, derived notify/session event classes, timer/idle handlers,
worker dispatch and native virtual overrides remain open C1 work.
[Scoped contexts](EVENT-CONTEXTS.md) provide Skip/propagation for supported
built-in and registered kinds, plus checked close/notify veto, without making
saved snapshots mutable.
Immutable event snapshots retain their existing contract.

## Example and verification

The [event-handlers example](../examples/event-handlers.lisp) constructs a handler
subclass with native state read in its AFTER hook. Separate window callbacks
toggle a target button's own event-processing flag. The button stays enabled
for input while its callback is gated:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-handlers.lisp \
  --eval '(wxlambda-event-handlers:start)'
```

Its `exercise` runs those same callbacks, verifies event counts and restores the
handler state before unbinding and destroying resources. The focused scenario
also verifies factories/class objects, slots/defaults, initializer rollback,
borrowed-destruction errors, stale handles and disabled-parent/child destruction:

```sh
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/event-handlers.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/event-handlers.lisp
python3 tools/coverage_inventory.py /path/to/wxWidgets-3.2.12/interface
python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. Rebuild the ABI 24
bridge alongside the bindings. The GUI runner includes this scenario; broader
platform and graphical Cadre verification remain separate. The API, this guide
and example are included in the [Cadre docset](DOCSET.md), installable under
`$XDG_DATA_HOME/docsets/wxlambda/` (default `~/.local/share/docsets/wxlambda/`).

The non-command [close family](CLOSE-EVENTS.md) now supports checked scoped veto
and real frame close requests; standalone handlers can receive synthetic close events.

Registered command/notify names and synchronous `process-event` extend the
standalone/window interface; see [notify payload and dispatch scope](NOTIFY-EVENTS.md).

See the [borrowed application-handler view](APPLICATION-HANDLER.md) for managed wxApp identity, inherited dispatch and lifetime restrictions. Session bindings and logging-off metadata are available; real OS delivery remains unverified.

[Session events](SESSION-EVENTS.md) extend the close family with query/end-session kinds, scoped logging-off metadata and isolated synthetic dispatch. Native OS session delivery remains unverified.
