# Borrowed application event handler

`current-application-handler` returns the session's unique `application-handler`
view of the bridge-owned wxApp. It is an `event-handler`, not a window or a
user-constructible application. It borrows native ownership; only application
session cleanup destroys wxApp. Repeated lookups return the same Lisp object,
including inside callbacks. Lookup requires the active main-thread session.
The bridge supports one application session per process.

```lisp
(wxlambda:with-application ()
  (wxlambda:register-event-type :application-message :family :command)
  (let* ((app (wxlambda:current-application-handler))
         (binding (wxlambda:bind-event app :application-message
                    (lambda (event) (print (wxlambda:event-text event))))))
    (unwind-protect
         (progn
           (wxlambda:queue-event app :application-message :text "Hello")
           (wxlambda:process-pending-events))
      (wxlambda:unbind-event binding))))
```

The inherited generic binding, unbinding, queueing, synchronous dispatch,
handler liveness and handler-enabled SETF operations apply to this view.
Observer callbacks still Skip, so delivery does not imply a true PROCESS-EVENT
handled result. Contexts and notify Veto/Allow retain their existing lifetime
and snapshot rules. `event-source` is the cached application view when it is
the binding receiver. Supported types remain built-ins and registered command/
notify types; no new native event families are introduced.

`make-instance` on APPLICATION-HANDLER or its subclasses signals an error;
MAKE-EVENT-HANDLER cannot manufacture it either. Subclassing cannot replace the
managed native wxApp or install virtual overrides. DESTROY-EVENT-HANDLER rejects
the borrowed view before deleting anything, including during a callback.
Native application identity uses an opaque handle distinct from windows and
owned standalone handlers. No native pointer is public.

HANDLER-ENABLED-P reads or writes wxApp's own wxEvtHandler enabled flag. Disabling
it can suppress application event processing and delay application-queued work,
including deferred receiver disposal, potentially until session cleanup. Restore it with UNWIND-PROTECT
when temporarily changing it. It is independent of RUN's loop state.
Bindings are removed before native application cleanup. A saved view cannot
perform native operations outside the session; these signal NATIVE-ERROR.
Saved immutable event snapshots remain readable, including their source view,
without implying that the source is still live.

This is a bounded wxWidgets 3.2.12 GetInstance/wxTheApp mapping. Application
construction, subclass hooks, traits, top-window operations, replacement of an
external wxApp and native application overrides remain unbound. Session-event
bindings and logging-off metadata are available; real OS delivery remains
unverified. In
particular, native query/end-session events route to wxApp and default
END_SESSION can exit the process. Synthetic session events are restricted to isolated owned handlers;
application/window receivers reject emission before event creation. See
[session events](SESSION-EVENTS.md) for routing restrictions and native policy.

Run the example (it queues a message, verifies delivery, unbinds and exits):

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/application-handler.lisp --eval '(wxlambda-application-handler:start)'
```

`tests/application-handler.lisp` also checks cached identity, inherited notify
dispatch and final veto, disabling/restoration, construction and destruction
rejection, binding cleanup, expired native access and saved snapshots.

Build/check the docset with `python3 tools/docset.py build` and `check`.
Install with `python3 tools/docset.py install` under
`$XDG_DATA_HOME/docsets/wxlambda/` (default
`~/.local/share/docsets/wxlambda/`) for Cadre discovery, API lookup and examples.
See [docset installation](DOCSET.md), [handlers](EVENT-HANDLERS.md) and
[notify events](NOTIFY-EVENTS.md).
