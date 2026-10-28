# Command-event bindings

This C1 slice adds independent observer bindings on standalone `event-handler`
objects and borrowed window identities. It covers `:click` (`wxEVT_BUTTON`),
`:text-change` (`wxEVT_TEXT`), `:toggle` (`wxEVT_CHECKBOX`) and `:selection`
(`wxEVT_CHOICE`), using the wxWidgets 3.2.12 baseline. The bridge is ABI 15.
Optional [scoped contexts](EVENT-CONTEXTS.md) now support Skip/propagation;
close-veto support is now available through the [close family](CLOSE-EVENTS.md).
Notify/session and arbitrary native event families remain open.

## Register and remove independent observers

```lisp
(defvar receiver (wxlambda:make-event-handler))
(defvar subscription
  (wxlambda:bind-event receiver :click
    (lambda (event)
      (format t "Receiver ~S; command ID ~D~%"
              (wxlambda:event-source event) (wxlambda:event-id event)))
    :id 40 :last-id 49))
(wxlambda:binding-active-p subscription) ; T
(wxlambda:queue-event receiver :click :id 42)
(wxlambda:process-pending-events)
(wxlambda:unbind-event subscription)     ; T
(wxlambda:unbind-event subscription)     ; NIL
```

Use these forms inside an active main-thread `with-application` session. Every
call to `bind-event` returns a distinct opaque, read-only `event-binding` value;
`event-binding-p` tests its type without consulting native state. There is no
public constructor or callback-function slot on that value. Keep it to remove
or query exactly that registration. Bindings may share a receiver, kind, ID filter
and callback function; none replaces another binding.

An omitted/NIL `:id` matches any command ID. A supplied ID matches exactly unless
`:last-id` specifies an inclusive range. IDs must fit signed 32-bit integers;
`-1` is reserved for native `wxID_ANY`, represented by NIL in binding filters.
`:last-id` requires `:id` and must be at least as large. Invalid registration
releases its temporary Lisp roots. Native unbinding uses the exact receiver,
event tag, ID range and native binding identity.

`unbind-event` is safe from inside the binding's own callback or a peer callback.
It returns T for an active removed registration, NIL after removal/receiver
expiration. The current callback can finish, but removed callbacks cannot receive
later deliveries. The native callback object stays alive until its current call
returns. Relative callback order follows wxWidgets; no Lisp ordering guarantee
is provided, including for bindings added during dispatch.

## Observer and source semantics

Each observer callback takes one immutable `event` snapshot. With
`:with-context t`, a generic callback instead receives `(snapshot context)` and
starts unskipped; it must explicitly set Skip to continue routing. Generic bindings and the
control-specific `on-click`, `on-text-change`, `on-toggle` and `on-selection`
callbacks are observers by default: their native hooks call `Skip` after delivery, including
when no control-specific callback is installed. Remaining matching native
handlers and normal command-parent routing can therefore continue. The
control-specific APIs still provide one replacement slot per control; generic
registrations coexist independently and are removed with `unbind-event`.

The default observer policy is unchanged. Contextual bindings can now consume
commands and control Skip/propagation levels through borrowed, callback-scoped
access. Close veto is now supported only for current close contexts. Notify/session
families and native virtual overrides remain missing; do not infer full wxEvent coverage from these command APIs.
`handler-enabled-p` still controls only the receiving handler's own processing.

`event-source` is the **Lisp receiver on which that callback was registered**.
For a parent binding that observes a child's command, it is the parent, not
necessarily the native originating child. Native origin-object lookup is not
exposed. `event-id` copies the native command ID; it is not a globally unique
event or native handle. Existing control-specific snapshots likewise carry their
registered control as source. Snapshots and copied strings survive callbacks,
unbinding and native receiver destruction, without requiring a session to read.

## Queue copied synthetic events

```lisp
(wxlambda:queue-event receiver :text-change :id 42 :text "Copied Unicode λ")
(wxlambda:queue-event receiver :toggle :checked-p t)
(wxlambda:queue-event receiver :selection :selection 3)
```

`queue-event` returns the supplied handler and queues a fresh `wxCommandEvent`
using `wxEvtHandler::QueueEvent`. It works on exposed standalone handlers and
windows, on the main thread only. `:id` defaults to 0. `:text` defaults to an
empty string and applies only to text-change events; explicit UTF-8 byte counts
preserve Unicode, empty strings and embedded NULs. Non-default payloads for the
wrong kind are rejected. `:checked-p` is a generalized boolean for toggles;
`:selection` is NIL (no selection) or a nonnegative signed-32-bit index.

Queuing only supplies event data: it does not change a text control's contents,
checkbox state or choice selection. Use generic SETF properties to change those
values and optionally notify. Synthetic selections are payload indices, not
validated against a choice's items. Native origin override, arbitrary custom
event classes, userData and worker-to-GUI dispatch are not implemented. Synthetic `:close` events use a
separate native event class and may run default frame closing; see [close events](CLOSE-EVENTS.md).

## Lifetime, errors and cleanup

Native weak references invalidate bindings when receivers disappear. Lisp roots
for generic callbacks are reaped when RUN, a pump or a notifying setter returns,
after explicit destruction, or when `binding-active-p` is called. A closed
window during a still-blocking RUN may retain roots until one of these boundaries
or explicit unbinding. Session cleanup releases remaining bindings. This does
not alter the control-specific callback-root rule: explicitly unbind those
callbacks before destruction or they remain rooted until session cleanup.

Disposal requested during a native-to-Lisp callback marks the target identity
invalid immediately and defers actual deletion through application `CallAfter`.
This lets native event and notifying-setter stacks return before their receiver
is deleted. Descendants follow their actual parent/window destruction lifetime.
Already queued events for a pending-disposal target cannot invoke its bindings.
If the application exits before deferred deletion runs, session cleanup owns
remaining native resources. Outside callbacks, owned-handler destruction remains
immediate and windows retain their existing native destruction behavior.

Inactive sessions signal `native-error` for registration, queuing, unbinding and
liveness queries, including before native-library loading. Stale/deleting
receivers reject operations; unbinding an expired registration returns NIL during
the active session. Unsupported Lisp receiver types signal CLOS dispatch errors.
Callback errors are contained at the native boundary and re-signaled after the
native call returns; event-pump reentrancy is still rejected. Initializer rollback
removes new generic registrations and callback roots across nested resources.

## Runnable example and verification

The [event-bindings example](../examples/event-bindings.lisp) installs two
observers on a standalone handler. A button queues an ID-42 notification; its
reusable `exercise` verifies that removing one subscription leaves the other:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-bindings.lisp \
  --eval '(wxlambda-event-bindings:start)'
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/event-bindings.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/event-bindings.lisp
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. Focused tests cover ID
filters, duplicate registrations, all four payload kinds, text copying, self/peer
unbinding, error recovery, disabled handlers, initializer rollback, parent/child
observer routing and disposal during queued and synchronous setter callbacks.
The API, guide and example are indexed in the [Cadre docset](DOCSET.md), installable
under `$XDG_DATA_HOME/docsets/wxlambda/` (default `~/.local/share/docsets/wxlambda/`).
Graphical Cadre and hosted-platform GUI verification remain separate checks.
