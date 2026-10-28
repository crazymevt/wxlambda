# Scoped event IDs and routing

ABI 29 extends EVENT-ID to a generic reader for saved immutable snapshots and
current innermost contexts. Context-only SETF maps wxEvent::SetId. Supported
scope is the existing seven built-in kinds and registered command/notify types,
using the wxWidgets 3.2.12 baseline. No native event ownership is transferred.

SETF accepts signed 32-bit integers and returns the assigned integer. Native
GetId uses status plus a separate output, so -1 is a valid ID rather than an
error sentinel. Non-integer/out-of-range assignments signal TYPE-ERROR before
FFI. Direct SetId accepts -1 as native metadata. Binding filters and initial
synthetic IDs still reserve -1: use NIL for an unfiltered binding, and another
initial ID for QUEUE-EVENT/PROCESS-EVENT. A mutated event ID of -1 does not make
all exact filters match; an unfiltered binding still matches any event ID.

Changing an event ID affects later native matching against that event, including
parent exact-ID and inclusive-range filters. It does not change the native
window's own ID, change binding filters, restart dispatch or re-run handlers
already visited. A consuming contextual callback still stops normal delivery;
set EVENT-SKIPPED-P true when forwarding is intended. Native propagation rules
still apply. Do not use ID mutation as a substitute for explicit Skip/propagation
policy. This API does not promise local handler callback order.

A snapshot captures the ID before its callback. Mutating the context never
changes an earlier snapshot. A later callback can capture the new ID;
EVENT-SOURCE remains the Lisp receiver on which that callback was registered,
not a newly inferred native origin. Saved snapshots remain readable after
source/session cleanup. There is no snapshot writer. Native context access
requires an active main-thread session and the innermost current callback;
expired or suspended outer contexts signal NATIVE-ERROR.

```lisp
(wxlambda:bind-event child :routed-command
  (lambda (event context)
    (assert (= 10 (wxlambda:event-id event)))
    (setf (wxlambda:event-id context) 42
          (wxlambda:event-skipped-p context) t))
  :id 10 :with-context t)
;; A parent binding filtered to ID 42 can now match the forwarded event.
```

The complete runnable example constructs the child/parent, registers the custom
command, verifies both snapshots and releases its bindings/windows:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-routing.lisp --eval '(wxlambda-event-routing:start)'
```

The regression verifies signed bounds and -1, malformed assignments, all existing
event families, unchanged snapshots, nested-context suspension and expiration,
parent exact/range/wildcard matching, consumption and queued delivery. It does
not generate native OS logout/shutdown events. Synthetic session events retain
their isolated owned-handler restrictions.

Native type mutation, origin access/mutation, categories/userData, persistent
event ownership/cloning, derived notify classes, pushed handlers, timer/idle and
worker dispatch remain C1 work. Next define safe native type/origin contracts;
a type tag must never authorize casting an incompatible native event object.

Build/check using `python3 tools/docset.py build` and `check`; install using
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre lookup and examples.
See [contexts](EVENT-CONTEXTS.md), [metadata](EVENT-METADATA.md) and
[docset installation](DOCSET.md).

[Native origin/type readers](EVENT-ORIGINS.md) now distinguish captured origin identity from binding receiver and provide current native type access; origin/arbitrary-tag writers and general wxObject wrappers remain open.

[Registered-tag mutation](EVENT-TAGS.md) now supports same-family SETF with actual native class checks; built-in/session tags and origin writes remain excluded.
