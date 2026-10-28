# Idle events and bounded idle work

ABI 31 adds built-in :IDLE with EVENT-FAMILY :IDLE and exact wxIdleEvent owned
construction/cloning, plus EVENT-MORE-REQUESTED-P, IDLE-MODE,
WINDOW-PROCESS-IDLE-P and WAKE-UP-IDLE. The baseline is wxWidgets 3.2.12.
This bounded foundation supports useful GUI updates; it does not make exhaustive
native utility/event parity a prerequisite for menus, dialogs or other controls.

## Idle delivery versus explicit event dispatch

BIND-EVENT accepts :IDLE on supported receivers, including the managed
application/window entry paths. Saved snapshots copy idle MoreRequested before
the callback. Native idle generation happens when the event loop/polling reaches
idle processing, not continuously merely because an application is quiet.

EVENT-MORE-REQUESTED-P reads captured/current/owned MoreRequested; it is NIL for
other event families. SETF calls RequestMore only on a live idle context or owned
idle event, normalizing a generalized boolean and returning the supplied value.
It requests further idle processing when the native loop is idle, not a busy-loop
or timing guarantee. Keep work bounded and stop requesting more when the work
list is empty. Owned flag changes prepare metadata for later copying; synthetic
dispatch is not a promise of automatic scheduling.

```lisp
(wxlambda:bind-event (wxlambda:current-application-handler) :idle
  (lambda (snapshot context)
    (declare (ignore snapshot))
    (when work (perform-one-small-piece (pop work)))
    (setf (wxlambda:event-more-requested-p context) (not (null work))))
  :with-context t)
```

MAKE-EVENT/MAKE-INSTANCE accepts optional :MORE-REQUESTED-P for idle construction.
Supplying it for a non-idle kind, even NIL, rejects and rolls back construction.
Clone source state wins inherited defaults; factories reject explicit clone
payload overrides including :MORE-REQUESTED-P. Keyword synthetic QUEUE-EVENT and
PROCESS-EVENT do not add this option: use an owned idle event to initialize it.

Owned idle resources support existing metadata, copy/post/transfer/normal/local
processing and lifecycle rules, including window stack-aware processing. Exact
wxIdleEvent is added to the bounded clone-class set; borrowed userData still
prevents native cloning. Idle is not a command event: command payload writes,
close veto/logging-off operations and registered-tag mutation reject under their
existing family/type rules. General native pointer adoption remains unbound.

## Automatic native policy

IDLE-MODE is a zero-argument generic reader with SETF for :ALL or :SPECIFIED,
mapping native wxIdleEvent GetMode/SetMode. It requires the active main-thread
GUI session. This is a process-wide native automatic-window idle policy, not a
property of one application wrapper or synthetic event. The native default is
:ALL; the actual pre-session value is saved and restored during native finish,
including body-error cleanup. Restore a temporary mode within a session with
UNWIND-PROTECT when other code must keep seeing its prior policy.

WINDOW-PROCESS-IDLE-P reads/writes only the receiver window's own
wxWS_EX_PROCESS_IDLE extra-style bit. SETF preserves all other extra-style bits
and returns the generalized boolean. It does not change global mode or report
effective delivery eligibility. Under :SPECIFIED the bit opts windows into
native idle delivery; :ALL is not restricted to windows with this bit. Native
visibility/ancestor rules and platform behavior also matter. The upstream
baseline allows hidden windows to receive idle, but excludes children of hidden
windows. The application can also receive global idle events independently of
which windows opt in. No raw arbitrary extra-style writer is added here.

```lisp
(setf (wxlambda:idle-mode) :specified
      (wxlambda:window-process-idle-p window) t)
(wxlambda:wake-up-idle)
```

WAKE-UP-IDLE returns T after scheduling a native wake. It does not pump events,
invoke callbacks inline, guarantee immediate delivery or expose worker-thread
wake/posting support. The binding deliberately requires the active GUI thread,
even though broader native thread use exists.

PROCESS-PENDING-EVENTS drains posted native pending queues; it does not generate
new native idle processing. An explicitly queued idle event can of course be
delivered from that queue. PROCESS-EVENTS uses native Yield plus ProcessIdle and
can generate automatic idle callbacks. Explicit synthetic idle dispatch bypasses
automatic mode/own-window-bit eligibility: it tests event handling, not native
idle scheduling policy. RequestMore is metadata and must not be mistaken for a
separate timer or a guaranteed number of callbacks per poll.

## Verification and next GUI feature

Run the bounded work-list example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/idle-events.lisp --eval '(wxlambda-idle-events:start)'
```

It requests only as much idle work as remains, restores policy and cleans up.
Validation distinguishes saved/current flags, exact idle owned/cloned resources,
synthetic versus native policy, preserved unrelated extra-style bits, native
wake versus inline callbacks and session-exit restoration. Existing event-context
lifetime and session-event isolation rules remain; :IDLE does not authorize
process-exiting session tags. Local checks do not prove hosted Windows/Linux GUI,
real OS-session delivery or graphical Cadre behavior.

No CanSend binding is added: that method is absent from the pinned wxWidgets
3.2.12 idle interface. Arbitrary event classes, retained origins and native
client/item pointers remain factual gaps but are not automatic blockers for
unrelated GUI breadth. **Frame menus/actions are available; standard dialogs are next**, adding only
prerequisites those workflows need. Use the form/editor,
tree/list manager and drawing/monitoring roadmap in
[GUI coverage priorities](WXWIDGETS-COVERAGE.md), not C1 exhaustion.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [owned events](OWNED-EVENTS.md), [event contexts](EVENT-CONTEXTS.md),
[pending dispatch](PENDING-EVENTS.md) and [docset installation](DOCSET.md).
