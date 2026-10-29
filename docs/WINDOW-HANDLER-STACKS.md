# Managed window event-handler stacks

ABI 31 adds WINDOW-EVENT-HANDLER, PUSH-EVENT-HANDLER, POP-EVENT-HANDLER,
REMOVE-EVENT-HANDLER, PROCESS-WINDOW-EVENT and PROCESS-WINDOW-EVENT-LOCALLY.
The wxWidgets 3.2.12 mapping supports bridge-managed windows and session-owned
standalone handlers, including Lisp subclasses. It adds no external handler
adoption, arbitrary SetEventHandler, raw pointer or native virtual Lisp callback.

## Stack ownership and identity

WINDOW-EVENT-HANDLER returns the top managed pushed handler as its canonical
Lisp wrapper. An empty managed stack returns the same window object passed to
the reader, matching native GetEventHandler's default self identity. The reader
checks that the native stack is known to the bridge; it does not wrap or adopt
an unknown native handler. It is read-only and may run during callbacks while
the window remains valid and is not being deleted.

PUSH-EVENT-HANDLER takes a live window and an unlinked session-owned standalone
handler, returns the handler, and places it above the prior top. Last pushed is
first in native stack traversal. Already attached or linked handlers reject;
window/application objects cannot be pushed as peers. Pushing does not move the
native unique ownership registry entry: the bridge session still owns the
handler and strongly roots its canonical wrapper. The window does not become
its deleting owner merely because it appears on the native event stack.

```lisp
(wxlambda:push-event-handler window handler)
(assert (eq handler (wxlambda:window-event-handler window)))
(wxlambda:pop-event-handler window) ; handler stays alive and unlinked
```

POP-EVENT-HANDLER without DELETE detaches and returns the top handler, alive and
unlinked. With `:delete t`, bridge ownership is released, native PopEventHandler
performs deletion, Lisp canonical roots/bindings are cleared, and NIL is returned.
The former wrapper is no longer live; native access and repeated destruction
reject. Popping an empty stack is an error, including with DELETE true.

REMOVE-EVENT-HANDLER detaches a specified live managed handler without deletion,
returning T if it belonged to that window and NIL otherwise. A live detached
handler or one attached to another window is a safe nonmember probe: no native
assertion is triggered and no stack changes. Wrong borrowed peers and destroyed
handlers reject instead of being treated as absent. Removal preserves other
stack entries, the native owned registry and the handler's own pending queue.

All structural stack operations require an active main-thread session at a
non-dispatch boundary. Callback, native pump and RUN use reject before mutation.
This includes pop/remove attempts that would be empty or absent. General
NEXT-HANDLER/PREVIOUS-HANDLER peer reads and SETF, and UNLINK-EVENT-HANDLER, reject
attached nodes so the standalone protocol cannot expose or edit the borrowed
window tail. Use the window stack API to inspect/detach them. HANDLER-UNLINKED-P
is the exception: it can read attached owned nodes and returns NIL.

## Window destruction and explicit handler destruction

All supported concrete native window kinds, including text/choice dialogs, use
an internal managed destructor that
pops managed handlers with native deletion disabled before wxWindowBase teardown.
Deleting a window or an ancestor therefore detaches pushed handlers; they remain
alive, unlinked and session-owned. Their own pending queues remain with them and
can deliver after the window is gone; window teardown does not cancel those queues.
This cleanup does not depend on a destroy-event
observer, its Skip choice or HANDLER-ENABLED-P. These internal native subclasses
do not add public CLOS classes or expose native virtual overrides.

This policy matters because the native window destructor requires pushed
handlers to have been removed; it does not act as their deleting owner. The
bridge also detaches managed stacks before application session window cleanup.
A requested asynchronous window destruction may leave the stack attached until
actual native teardown; do not equate a destruction request with completed
handler detachment.

DESTROY-EVENT-HANDLER on an attached handler removes it from its window before
native deletion when called outside callbacks. During callbacks, linked/attached
handler destruction rejects so an active traversal is not structurally edited.
Unlinked handler callback disposal retains its prior deferral behavior.
Constructor rollback removes/deletes a failed owned handler even if its AFTER
hook attached it; a failed window's eventual native destruction detaches pushed
handlers. Resource rollback is not arbitrary restoration of external stack/graph
edits made by user hooks. No GC finalizer or cross-session reuse is introduced.

## Explicit window routing

PROCESS-WINDOW-EVENT and PROCESS-WINDOW-EVENT-LOCALLY take a window and a live
OWNED-EVENT, process an independent detached copy, and return handled-p and
final-vetoed-p from that copy. The source owned event stays alive/unchanged and
can be destroyed during callbacks. Copy origin is the window, while a pushed
handler's callback snapshot source is that handler's canonical Lisp wrapper.
No payload override keywords are accepted.

PROCESS-WINDOW-EVENT enters native GetEventHandler routing and can continue from
the stack through the window and normal parent/application fallback.
PROCESS-WINDOW-EVENT-LOCALLY searches this window and its pushed handlers,
including applicable native validators/local pre-processing, without normal
upward window propagation. Native local static/default handlers may still run,
including frame-close behavior; local routing is not observer-only. Observer Skip/consumption and nearer callback-error
boundaries retain their existing meaning; a callback can run while handled is
NIL, and veto is independent of handled.

Existing PROCESS-EVENT, PROCESS-EVENT-LOCALLY and queued/post/transfer operations
on a window deliberately retain wxEvtHandler receiver semantics and bypass its
pushed stack. Use the explicit synchronous window operations when that stack
must participate. They do not change where native pending objects are queued,
move pending events between nodes, install Lisp callback chains or promise
callback order within a node. Native window input routing still follows its own
wxWidgets entry paths.

## Session and payload restrictions

Synthetic query/end-session delivery and session bindings are not allowed on
attached pushed handlers. PUSH rejects a handler with active session bindings or
queued session events before attachment. Native standalone session tracking is
cleared when those events are processed or deleted; it introduces no worker
posting or Lisp native virtual hook. These restrictions prevent the borrowed
window tail from becoming a route to process-exiting wxApp session defaults.
Native app session delivery remains separately unverified.

The owned-event copied-dispatch and lazy wxEVT_TEXT/text-entry receiver guards
also apply to the explicit window operations. No native class replacement,
arbitrary tag mutation, borrowed client/userData ownership or retained origin
pointer is added. Window SetEventHandler and adoption of external native stack
entries remain unbound.

Run the complete example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/window-handler-stacks.lisp --eval '(wxlambda-window-handler-stacks:start)'
```

The original eight-widget focused regression covers stack-aware explicit routing versus generic window
bypass, pop/remove/deleting-pop lifetime, nonmember/stale probes, all eight widget
destructors, parent-cascade detachment with window event processing disabled,
pending delivery after pop/window deletion, session guards and rollback in both
construction directions. wxEVT_DESTROY is not publicly bindable, so consuming a
real destroy notification is not directly tested; teardown independence is
structural/native review rather than a claimed destroy-callback test. A destroy
request still differs from actual native detachment. Local checks do not establish hosted
Windows/Linux GUI or graphical Cadre behavior. Remaining C1 work includes
external/native stack adoption, arbitrary SetEventHandler, origin retention,
arbitrary native client/userData ownership, timer events and worker dispatch.
Bounded idle scheduling and Lisp-rooted handler/binding data are now supported.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [standalone chains](HANDLER-CHAINS.md), [handler lifetime](EVENT-HANDLERS.md),
[owned events](OWNED-EVENTS.md) and [docset installation](DOCSET.md).

[Handler Lisp client data](HANDLER-CLIENT-DATA.md) now retains EQ values through
a native typed token adapter with safe foreign-mode rejection. General native event/client/item pointer ownership remains unbound; per-binding
Lisp USER-DATA is separately supported.

## Safe deferred structural boundary (ABI43)

[Deferred UI actions](DEFERRED-UI-ACTIONS.md) run after native callbacks and
operations return. Their outer action body is an explicit safe structural
boundary even under RUN/full polling; the quiescent structural operations in
this guide can run there. Nested event callbacks retain their existing guards,
and nested RUN/pumps/finish remain forbidden. Optional owner/cancel/error rules
are documented in that guide.
