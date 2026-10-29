# Bounded UI timers

ABI44 adds main-thread periodic and one-shot UI work with opaque `ui-timer`
tickets. It uses the same safe idle phase as deferred UI actions, rather than
calling Lisp inside native wxTimer::Notify.

```lisp
(let ((timer nil) (updates 0))
  (setf timer
        (wxlambda:start-ui-timer
          (lambda ()
            (incf updates)
            (setf (wxlambda:status-text status) (format nil "Update ~D" updates))
            (when (= updates 10) (wxlambda:cancel-ui-timer timer)))
          250 :owner frame)))

(wxlambda:start-ui-timer (lambda () (setf (wxlambda:status-text status) "Once"))
                        1000 :owner frame :one-shot t)
```

FUNCTION takes no arguments; MILLISECONDS is an integer from 1 through 86400000
(one day). Default mode is periodic; non-NIL `:one-shot` selects one delivery.
At most256 live timer registrations are supported. Construction returns a new
immutable ticket and never calls the function inline. Restart, interval mutation,
raw native IDs/owners, wxTimer event receivers and timer-event snapshots are not
exposed. To change an interval, cancel and create a new ticket. `ui-timer-p` is a
pure type predicate and remains true after retirement, including outside a session.
No public ticket constructor or token accessor exists; this is a scheduler identity,
not a CLOS wxTimer/event-handler wrapper or public native Notify override.

## Delivery, coalescing and ordering

A native notification marks at most one due tick. Repeated expirations before
delivery coalesce; there is no missed-tick count or elapsed-interval catch-up.
The safe scheduler merges due timers and ordinary deferred actions by their
first due/enqueue sequence, delivering at most one item per native idle iteration.
This avoids starvation from a continuously due timer or an action backlog. Timer
due work is separate from the1024-action queue, so a full action queue cannot
silently discard a one-shot tick. Expired-owner cleanup can retire records before
ordinary work; it invokes no user function.

RUN/full `process-events` drives delivery. A full poll can perform several idle
iterations; `process-pending-events` does not run timer functions or guarantee
OS timer notification processing. Functions wait while a modal/dialog/clipboard
or guarded callback/native operation is active, while another safe UI function
runs, during shutdown or while a callback error is pending. Native ticks may
accumulate one due notification during those waits. A timer function can open a
modal, but that modal cannot deliver another timer/deferred function recursively.
Use END-MODAL in the current dialog callback; a waiting timer function cannot
dismiss an already active modal.

Functions run on the GUI thread in the safe structural phase: status/control
updates, modal/clipboard work, window destruction and structural operations are
available subject to normal ownership checks. Newly nested event callbacks retain
their guards. Recursive RUN/pumps/session finish reject; STOP may leave RUN.
Return values are ignored. Long functions block GUI progress; these are not
worker threads, high-resolution clocks, deadlines or real-time scheduling.
Native resolution, event-loop load and coalescing can delay updates. A monotonic
initial lower bound filters stale early notifications assigned to a newly started
timer after native ID reuse; no strict cadence or per-interval timing guarantee.

## Activity, ownership, cancellation and errors

`ui-timer-active-p` queries the logical live registration. A stopped native
one-shot remains active while its due function awaits safe delivery; its identity
and queue root retire before invocation, so it is inactive inside its function.
A periodic timer remains active inside its function until canceled/owner expiry.
`cancel-ui-timer` stops native notifications, removes due work and releases the
Lisp closure root, returning T once and NIL if already removed. It is safe to
cancel the current periodic timer or another timer from a function/callback.
Cancel cannot interrupt the current function. The executing function remains
locally rooted even if its registration is removed while a nested modal runs.

Optional `:owner` is a checked native weak window reference. Native pending owner/
ancestor deletion counts as expiry. Observed expiry stops/retires the timer without
running the function; queries/reaping/safe idle release native storage and Lisp
roots. Notify can stop/mark an expired owner but never deletes itself. Expiry is
observed, not a synchronous Lisp call from a widget destructor. The function may
capture other values/widgets; an owner does not keep native widgets alive or
protect arbitrary captured identities. Without an owner, the timer belongs to
the application session and can outlive individual frames/RUN returns.

Both active/cancel operations require the current main-thread session. Session
finish stops/deletes all native timers before window/toolkit teardown and clears
Lisp roots on exit. Failed native start removes its prepared closure root. Managed
widget initialization rollback also reaps timers owned by the failed widget.
Do not process GUI events before managed construction has completed.

An escaping periodic function error cancels that timer to avoid repeated failures;
errors/nonlocal exits use the existing contained safe-work error boundary and
re-signal at outer RUN/full poll. Other pending work pauses until recovery within
the session. One-shot identity was already removed. Locally handled errors can
return normally and leave a periodic registration active. Cancellation is not
transactional rollback of completed UI/IO effects. Capture event snapshots/values,
not expired borrowed event contexts.

## Backend scope and example

The baseline and verified runtime is wxWidgets3.2.12; CMake requires wxUSE_TIMER=1.
On macOS timer creation currently accepts **only wxWidgets3.2.12**, not arbitrary
newer versions. The audited Cocoa cancellation path checks an invalidated queued
CFRunLoopTimer before touching its old data. Official3.3.0/3.3.1 branches lack that
check, so numeric version ordering is not a safety test. Unverified versions reject
start with NATIVE-ERROR; other bindings retain their own existing version policy.
Other timer backends are source-reviewed but hosted Windows/Linux GUI runs remain
outstanding. Re-audit native cancellation before widening the macOS gate.

Native timers internally run continuously. The bridge ignores stale early first
notifications, then stops logical one-shots at their first valid notification and
retires them at delivery. It does not expose native IsOneShot or native timer event
semantics. Native cancellation/source review uses
[3.2.12 Cocoa timer](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/osx/core/timer.cpp),
[3.3.1 Cocoa timer](https://github.com/wxWidgets/wxWidgets/blob/v3.3.1/src/osx/core/timer.cpp),
[GTK timer](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/gtk/timer.cpp) and
[MSW timer](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/msw/timer.cpp).

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/periodic-status.lisp --eval '(wxlambda-periodic-status:start)'
```

The example updates a label/status bar periodically and supports Pause/Resume by
canceling/creating registrations. EXERCISE uses real native timers and self-cancels
after two updates. Tests cover real one-shot/periodic delivery, modal exclusion,
self-cancel, RUN, roots/owner/rollback/error/nonlocal/shutdown on both Lisps. Native
probes independently force coalescing/ordering, first-tick filtering, due one-shot
activity, expiry cleanup, capacity and worker rejection. Physical input, timer
precision/deadline metrics, hosted OS GUI, OS clipboard interoperability, allocation
race injection and graphical Cadre remain unverified. No system clipboard text is
read/replaced by these automatic tests.

See [deferred actions](DEFERRED-UI-ACTIONS.md), [coverage](WXWIDGETS-COVERAGE.md)
and [Cadre installation](DOCSET.md).
