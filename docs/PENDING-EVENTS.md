# Pending-event copies, transfer and local processing

ABI 28 adds POST-EVENT, TRANSFER-EVENT, PROCESS-EVENT-LOCALLY and
DELETE-PENDING-EVENTS as generic handler operations. They build on detached
OWNED-EVENT resources and the wxWidgets 3.2.12 event-handler interface. This
batch distinguishes copy delivery from genuine native ownership handoff.

| Operation | Input resource after success | Native operation | Delivery |
| --- | --- | --- | --- |
| POST-EVENT | Still live and unchanged | GUI wxPostEvent / AddPendingEvent copy | Queued |
| QUEUE-EVENT with owned input | Still live and unchanged | QueueEvent with independent clone | Queued |
| TRANSFER-EVENT | Consumed; no longer owned by the bridge | wxQueueEvent / QueueEvent ownership handoff | Queued |
| PROCESS-EVENT | Still live and unchanged | ProcessEvent on independent clone | Synchronous normal routing |
| PROCESS-EVENT-LOCALLY | Still live and unchanged | ProcessEventLocally on independent clone | Synchronous local routing |

The four new operations require an active main-thread GUI session. They accept
event-handler/window receivers as described below; event-delivery operations
accept a live OWNED-EVENT rather than a kind keyword or snapshot. No payload
override keywords are supported. Mutate the owned resource first, or clone it
when a distinct reusable input is needed.

## Posting a GUI copy

POST-EVENT returns RECEIVER after posting a detached copy using wxPostEvent,
which uses AddPendingEvent in this GUI application. It does not process callbacks
before returning. Its input remains live and unchanged and can be changed or
destroyed immediately afterward without changing the queued copy. The receiver
is the copy's native origin, just as for owned QUEUE-EVENT dispatch.

This mapping deliberately requires the existing GUI session; wxPostEvent's
non-GUI immediate-dispatch behavior is not exposed. AddPendingEvent string
copying is not a worker-thread facility here. Worker-to-GUI posting remains
unbound, including any broader thread-safety guarantees.

```lisp
(wxlambda:post-event receiver message)
(assert (wxlambda:event-alive-p message))
(wxlambda:process-pending-events) ; existing application-wide pending dispatch
```

## Transferring native ownership

TRANSFER-EVENT returns RECEIVER after handing the actual owned native object to
wxQueueEvent. It validates the receiver, supported event and receiver-family
restrictions before consuming the input. A validation failure leaves the owned
resource live so it can be corrected, copied or destroyed normally.

After preflight, the native object is removed from the bridge-owned registry and
handed off to wxWidgets. EVENT-ALIVE-P on the original wrapper then returns NIL
during the active session; native property access, cloning and DESTROY-EVENT on
that consumed wrapper reject. Its integer handle is not another owner's public
receiver. The receiver becomes the transferred event's native origin.

There is no rollback after handoff or delivery guarantee. If the destination is
later destroyed, pending events are deleted, or native delivery otherwise does
not happen, ownership is not returned to the consumed wrapper. A failure after
the handoff boundary does not restore bridge ownership either. Use POST-EVENT or
QUEUE-EVENT when the input must remain reusable.

```lisp
(wxlambda:transfer-event receiver (wxlambda:clone-event message))
(assert (wxlambda:event-alive-p message)) ; original message was not transferred
```

Callbacks can retain immutable snapshots or clone their current context into a
new detached resource. They cannot regain ownership of the consumed wrapper.
The bridge never exposes the native event pointer during transfer.

## Synchronous local processing

PROCESS-EVENT-LOCALLY returns two values: native handled-p and final-vetoed-p
from an independent detached copy. It leaves the input alive and unchanged.
Like native ProcessEventLocally, it tries the receiver and native chained
handlers, including local pre-processing such as TryBefore/validators. Native
local static/default handlers can still run, including frame-close behavior.
Local processing is not an observer-only or no-default-action mode. It does
not run the normal outer global-filter and parent/application fallback path of
ProcessEvent. Managed standalone [handler chains](HANDLER-CHAINS.md) are now
available; dedicated managed window stacks are also bound, while external/native stack
adoption, arbitrary SetEventHandler and virtual overrides remain unbound.

An observing callback can run and Skip while handled-p is NIL. Final veto state
is independent of handled-p. Nested current-context lifetime rules and the nearer
callback-error boundary remain the same as PROCESS-EVENT. Local processing is
synchronous; it neither pumps unrelated pending queues nor converts them into
local delivery. Choose PROCESS-EVENT for normal native parent/application routing.

## Deleting one handler's pending events

DELETE-PENDING-EVENTS returns T after deleting every pending native event on the
selected receiver. It does not execute callbacks or remove bindings. Later posts
and queues on that handler still work. It does not delete another handler's
queue, report a deletion count, restore transferred resources or perform
application-wide queue deletion. An empty target queue is allowed.

The borrowed APPLICATION-HANDLER rejects this operation: application-queued work
includes bridge deferred cleanup and must not be discarded through this API.
The operation also rejects during RUN, a native pump or any callback. Call it at
a non-dispatch boundary, not from the delivery being cancelled.

The wrapper synchronizes the target with wxApp's pending-handler list after
native DeletePendingEvents. The implementation uses the native
RemovePendingEventHandler management method because DeletePendingEvents alone
does not remove the list entry. That method exists in the implementation header,
but is absent from the pinned documentation-interface inventory; it is an
implementation dependency for C7 reconciliation, not a new public Lisp binding.

Per-handler ProcessPendingEvents remains unbound. Native code requires a known
nonempty target queue, and this bridge does not expose a safe per-handler pending
query. Existing PROCESS-PENDING-EVENTS remains the application-wide dispatcher.
wxAppConsole::DeletePendingEvents also remains unbound; target-handler deletion
must not be mistaken for application-wide pending cancellation.

## Shared restrictions and verification

All new delivery modes retain existing supported exact native classes, detached
origin policy and session isolation. Session event delivery still requires an
isolated owned standalone receiver; application/window targets reject before
handoff or posting. Copied owned :TEXT-CHANGE delivery to text-entry receivers
rejects before emission, preventing lazy native GetString from shadowing the
owned payload. Transfer applies the same text-entry restriction. These APIs add
no arbitrary origin retention, raw pointers, native class replacement, userdata/
client ownership, real OS session delivery or worker-thread posting.

Run the example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/pending-events.lisp --eval '(wxlambda-pending-events:start)'
```

It demonstrates the distinct copy/transfer/local/cancellation semantics using
owned events and releases resources. Regression validation should distinguish
consumed handles from surviving copies, preflight failure from handoff, local
routing from normal parent routing, handler-specific cancellation from other
pending queues, and callback/pump/application restrictions. Local results do not
validate hosted Windows/Linux GUI behavior, real OS session events or graphical
Cadre. Remaining C1 gaps include origin retention, broader native classes,
client/userdata ownership, application-wide cancellation, per-handler pending
queries, pushed handlers, timer/idle and worker dispatch.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [owned events](OWNED-EVENTS.md), [application dispatch](APPLICATION-EVENTS.md),
[event bindings](EVENT-BINDINGS.md) and [docset installation](DOCSET.md).

[Window stack routing](WINDOW-HANDLER-STACKS.md) is explicit: generic and queued
window operations retain wxEvtHandler stack-bypass behavior; PROCESS-WINDOW-EVENT
and PROCESS-WINDOW-EVENT-LOCALLY enter managed pushed handlers synchronously.
