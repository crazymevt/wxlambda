# Event metadata and mutable CanVeto

Native bridge ABI 27 adds EVENT-TIMESTAMP and EVENT-COMMAND-P readers for
immutable snapshots and current innermost event contexts. EVENT-TIMESTAMP also
has a context-only SETF writer. EVENT-CAN-VETO-P now has a context-only SETF
writer for wxCloseEvent: window-close, query-end-session and end-session events.
The baseline is wxWidgets 3.2.12. These extend existing native event views;
they do not create persistent mutable native event objects.

## Timestamp and command classification

EVENT-TIMESTAMP returns the native signed C long timestamp, widened without
loss across the ABI. wxWidgets describes it as milliseconds since a fixed
moment, not necessarily the Unix epoch; do not convert it to a calendar date.
Native documentation warns that nonzero timestamps are normally supplied only
for mouse/key events, which this bridge does not yet bind. Synthetic supported
events start at zero. Negative values, including -1, are valid metadata and do
not double as native error sentinels.

SETF accepts a signed native C long integer (32 bits on Windows; typically
64 bits on 64-bit Unix). Lisp validation uses CFFI's native LONG size; native
validation repeats the range check before narrowing the fixed-width ABI value.
Out-of-range or non-integer values signal TYPE-ERROR. Changing a timestamp only
changes metadata: it neither schedules nor delays delivery. Synthetic queue and
process calls have no timestamp initialization keyword; change it in a current
context if needed.

EVENT-COMMAND-P copies/reads native IsCommandEvent. It is true for command and
notify-derived events and false for close-family events. It is read-only;
there is no classification writer or native event-class replacement.

## CanVeto is mutable native metadata

SETF EVENT-CAN-VETO-P maps directly to wxCloseEvent::SetCanVeto. It accepts a
generalized boolean and returns the supplied value. It can change false to
true, including an event that began non-vetoable. Every subsequent SETF
EVENT-VETOED-P still checks the *current* CanVeto flag before invoking Veto.
No CanVeto writer exists for snapshots or notify/command contexts; live access
on those non-close contexts rejects with NATIVE-ERROR. Notify veto capability
is intrinsic and cannot be changed by SetCanVeto.

Native GetVeto is CanVeto AND the stored veto bit. Setting CanVeto false hides
an existing veto without clearing that bit; setting it true again exposes the
stored veto. Explicitly clear veto while CanVeto is true if that is the intended
policy. A forced close starts with CanVeto false, but code that explicitly
changes this flag can change the result observed by native closing logic.
The flag is not proof that the OS permits cancelling shutdown; changing event
metadata does not change OS intent or guarantee that a veto will be honored.

Snapshots capture each callback's pre-callback timestamp, command classification
and CanVeto/veto state. Changes through a live context do not alter a snapshot
already captured. Later callbacks can capture changed native metadata. Native
callback ordering remains wxWidgets behavior, not a Lisp ordering guarantee.
All native reads/writes require the active main-thread session and the innermost
current context. Expired or suspended outer contexts signal NATIVE-ERROR; saved
snapshot readers need no native session and remain usable after cleanup.

```lisp
(wxlambda:bind-event receiver :close
  (lambda (event context)
    (assert (not (wxlambda:event-command-p event)))
    (setf (wxlambda:event-timestamp context) 123
          (wxlambda:event-can-veto-p context) t
          (wxlambda:event-vetoed-p context) t))
  :with-context t)
```

Run the isolated synthetic example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-metadata.lisp --eval '(wxlambda-event-metadata:start)'
```

The regression covers all seven built-in and both registered families, native
LONG extremes and negative timestamps, malformed/range errors, immutable
snapshots, later snapshot capture, stored-veto hiding/restoration, non-close
writer rejection and nested/expired context isolation. It does not emit real
OS session events or claim hosted Windows/Linux runtime verification.

Native origin/type mutation, persistent event ownership/cloning, userData,
derived notify classes, pushed handlers, mouse/key, timer/idle and worker dispatch
remain required C1 work. Next consider native type/origin access and broader event ownership.

Build/check with `python3 tools/docset.py build` and `check`. Install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [contexts](EVENT-CONTEXTS.md), [session events](SESSION-EVENTS.md) and
[docset installation](DOCSET.md).

[Scoped event IDs](EVENT-ROUTING.md) now provide current ID reads/SETF and document effects on later matching. Snapshots remain immutable.

[Native origin/type readers](EVENT-ORIGINS.md) now distinguish captured origin identity from binding receiver and provide current native type access; origin/arbitrary-tag writers and general wxObject wrappers remain open.

[Event categories](EVENT-CATEGORIES.md) now provide saved/current native category flags and compiled keyword mask lookup, including the actual native ALL mask. Selective pumping remains unbound.

[Owned events](OWNED-EVENTS.md) now provide detached native construction/cloning,
explicit disposal, mutable value fields and copied dispatch. Earlier immutable
snapshots and scoped contexts keep their lifetime rules. Arbitrary native classes,
origin retention and client/userData ownership remain open.
