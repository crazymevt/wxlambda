# Session events and logging-off metadata

The wxWidgets 3.2.12 close family includes `:query-end-session` and
`:end-session`, alongside window `:close`. EVENT-FAMILY returns `:close` for
all three. EVENT-TYPE returns their process-local native tags; immutable
snapshots retain kind, native type, ID, CanVeto, pre-callback veto and logging-off
metadata after callback and session cleanup. Native bridge ABI: 18.

Bind native session tags on CURRENT-APPLICATION-HANDLER with BIND-EVENT.
The operating system routes these events to wxApp; window receivers reject
session bindings. Existing observer callbacks Skip after delivery. Contextual
callbacks start unskipped and choose whether to delegate with EVENT-SKIPPED-P.
Native query/default policy and OS behavior remain in wxWidgets. Default
END_SESSION handling can terminate the process. An observer or callback error
that Skips can therefore reach that default. Do not use an observer to prevent
shutdown, and do not infer that a consumed event can cancel an irrevocable OS
shutdown. Any veto write requires EVENT-CAN-VETO-P true, including writes of NIL.

This slice installs/removes application bindings locally but does not trigger
real OS logout/shutdown. Native delivery and OS policy remain unverified on all
ports; the declaration mappings remain partial. In particular, support for
native session notifications varies by wxWidgets port. There is no managed
shutdown transaction, persistence hook, native wxApp override or guarantee that
all platforms deliver both events or permit veto.

## Logging-off access

EVENT-LOGGING-OFF-P is a generic reader for a saved session snapshot or the
current innermost session-event context. True means logging off; NIL means
system shutdown. SETF changes the current native session event's flag and returns
the supplied generalized boolean. This changes event metadata, not OS intent.
Snapshots are immutable and preserve pre-callback state. Non-session snapshots
signal a Lisp error; non-session/expired/suspended contexts signal NATIVE-ERROR.
Native context access requires the active main-thread session. No SETF method
exists for snapshots.

QUEUE-EVENT and PROCESS-EVENT accept `:logging-off-p` (default NIL) only for
session kinds, and `:can-veto-p` (default T) for all close-family kinds.
These are synthetic construction defaults, including CanVeto T for synthetic
END_SESSION; native OS events retain their actual flags and may be non-vetoable.
Initial `:vetoed-p` remains restricted to registered notify types. Session
construction rejects command text, integer, selection and toggle payloads.
PROCESS-EVENT returns native handled-p and final-vetoed-p independently.

## Isolated synthetic probes

Synthetic session dispatch/queueing requires an owned standalone EVENT-HANDLER.
The native bridge rejects application and window receivers before creating or
queueing the event. The internal standalone native handler suppresses wxApp
fallback specifically for the two session tags. This matters even with no
matching binding, a skipped observer or disabled handler: ordinary wxEvtHandler
TryAfter can otherwise forward an unhandled event to the application and run
its shutdown defaults. Other event kinds retain their native fallback behavior.
Synthetic probes never simulate OS delivery or change the real session.

```lisp
(wxlambda:with-application ()
  (let* ((receiver (wxlambda:make-event-handler))
         (binding (wxlambda:bind-event receiver :query-end-session
                    (lambda (event context)
                      (declare (ignore event))
                      (when (wxlambda:event-can-veto-p context)
                        (setf (wxlambda:event-vetoed-p context) t)))
                    :with-context t)))
    (unwind-protect
         (wxlambda:process-event receiver :query-end-session :logging-off-p t)
      (wxlambda:unbind-event binding)
      (wxlambda:destroy-event-handler receiver))))
```

Run the isolated example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/session-events.lisp --eval '(wxlambda-session-events:start)'
```

The regression also checks both kinds, unmatched/observer/disabled fallback
isolation, queue copies, native application binding removal without emission,
window binding rejection, synthetic receiver guards, snapshot independence,
nested context suspension/expiration and guarded veto. No real OS logout or
shutdown is used in automated checks.

[Event metadata](EVENT-METADATA.md) now adds timestamps, command classification
and current close-family SetCanVeto, including explicit false-to-true changes.
These do not grant OS shutdown cancellation. Persistent event objects/cloning,
mutable native IDs/types/origin, derived notify classes, pushed handlers,
timer/idle and worker dispatch remain required C1 work. Next define scoped ID
mutation and routing semantics before broader event ownership/families.

Build/check using `python3 tools/docset.py build` and `check`. Install using
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre lookup and examples.
See [application identity](APPLICATION-HANDLER.md), [close events](CLOSE-EVENTS.md)
and [docset installation](DOCSET.md).
