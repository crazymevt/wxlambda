# Activity indicators for busy workflows

ABI55 adds native-backed `activity-indicator`, generic `make-activity-indicator`
and generic `activity-running-p`/SETF. The indicator is a WINDOW subtype with a
native animation owned by its parent window.

```lisp
(defparameter indicator
  (wxlambda:make-activity-indicator parent :running-p nil))
(setf (wxlambda:activity-running-p indicator) t)
(wxlambda:activity-running-p indicator) ; native boolean state
(setf (wxlambda:activity-running-p indicator) nil)
```

`:parent` is required; `:running-p` defaults to NIL and accepts a generalized
boolean. Native state is verified after construction and each write. The reader
returns T/NIL; the writer returns its supplied value. Repeated true writes do not
accumulate starts, and one NIL write stops animation. Hidden or disabled controls
may retain running state; the state reports native animation activity, not a
promise that pixels are visible or changing.

Direct MAKE-INSTANCE subclasses support slot initargs, `:default-initargs`, native
properties in AFTER hooks and managed failure/root rollback. The factory is thin,
validates `:class`, and forwards subclass initargs. Native size/style/animation
appearance and frame cadence are port-specific. No standalone animation resource,
raw pointer, native timer/Notify event, progress percentage, artwork/style or
worker-thread API is exposed. Lisp subclasses do not override C++ methods.

Animation is feedback only: it does not run application work. Keep native GUI
processing responsive; a long blocking Lisp computation can stop visible updates.
Use existing deferred actions/UI timers or an application-specific worker model
with appropriate GUI-thread handoff rather than assuming this control starts a
background task.

## Main-thread and callback boundaries

All access requires a live control in the active main-thread GUI session. The
reader is callback-safe. Start/stop writes reject event callbacks, including
writes to a different indicator, and should be deferred:

```lisp
(wxlambda:on-click busy-button
  (lambda ()
    (wxlambda:defer-ui-action
      (lambda () (setf (wxlambda:activity-running-p indicator) t))
      :owner indicator)))
```

During a synchronous native start/stop operation, the shared Input guard protects
the receiver and ancestors from bridge destroy/close, rejects recursive writes,
and blocks nested GUI/RUN/pumps/finish, modal/clipboard entry and safe-idle delivery.
Native animation state belongs to the window; parent/session destruction releases
native animation resources and invalidates the Lisp wrapper normally. Owner-bound
deferred actions cancel if the owner disappears. There is no Lisp animation
callback root to release.

## Build and verification scope

CMake requires wxUSE_ACTIVITYINDICATOR=1 and wxUSE_GRAPHICS_CONTEXT=1; existing
TIMER support is also needed by the generic implementation. The installed
wxWidgets3.2.12 class is exported from adv despite the interface header's core
library annotation. Linkage remains core/base/adv.

Source audit covers Cocoa NSProgressIndicator, GTK spinner, and generic private
wxTimer/graphics rendering. Native appearance, enabled/hidden behavior, smoothness,
accessibility, hosted Windows/Linux/Qt and graphical Cadre remain unverified.
The deterministic example toggles busy feedback through owner-bound deferred
commands; it does not claim pixel/animation-fidelity testing. Native probes cover
raw state/type validation, repeated writes, synchronous operation/lifetime guards,
main-thread rejection and deletion while running. Lisp tests cover CLOS/defaults/
factory/rollback and callback deferral. No automatic test reads or overwrites
system clipboard contents.

Run [activity-indicator.lisp](../examples/activity-indicator.lisp) using its indexed
command in the docset. See [CLOS API](CLOS-API.md),
[deferred UI actions](DEFERRED-UI-ACTIONS.md), [UI timers](UI-TIMERS.md) and
[Cadre installation](DOCSET.md).

Native reference: [wxActivityIndicator](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/activityindicator.h).
