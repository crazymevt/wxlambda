# Bounded registered event-tag mutation

ABI 31 adds context-only SETF EVENT-NATIVE-TYPE. The reader still returns the
captured/current native type integer; assign an integer returned by EVENT-TYPE
for a name registered in the active session. Baseline: wxWidgets 3.2.12.

Both the current native tag and target must be registered command/notify tags.
The registry families must match, and native dynamic class checks must confirm
wxCommandEvent without wxNotifyEvent for command tags, or wxNotifyEvent for notify
tags. A changed tag never changes native C++ storage/class. Built-in sources and
targets reject, even if the apparent family matches. Window-close and both OS
session tags cannot be assigned or changed by this writer. These restrictions
preserve native cast compatibility and isolated synthetic session fallback.
Arbitrary native type mutation remains a coverage gap.

SETF accepts signed32 integers, validates registration/family/class natively and
returns the supplied integer. Unknown/built-in/cross-family types signal
NATIVE-ERROR before mutation; malformed/out-of-range values signal TYPE-ERROR.
Assigning the current registered type is allowed. A failed write leaves the
native tag unchanged. Snapshot SETF has no method. Native reads/writes require
the active main-thread session and innermost callback; expired or suspended outer
contexts signal NATIVE-ERROR.

Each callback snapshot reflects the actual current supported native tag, not an
assumption that it still equals the binding's registered kind. Earlier snapshots
retain their original EVENT-KIND and EVENT-NATIVE-TYPE. Binding identities and
Unbind retain their original registration tag. Changing a tag does not rebind,
restart dispatch or guarantee which remaining local callbacks will run. Native
dynamic/static lookup can match tags at different points; local callback order
is not a Lisp API promise. Explicit Skip and normal propagation rules still
control whether the event continues. A parent searches the changed tag when it
receives a forwarded command.

```lisp
(wxlambda:bind-event child :raw-tag
  (lambda (event context)
    (assert (eq :raw-tag (wxlambda:event-kind event)))
    (setf (wxlambda:event-native-type context)
          (wxlambda:event-type :translated-tag)
          (wxlambda:event-skipped-p context) t))
  :with-context t)
;; Both names must have been registered with :family :command.
```

Text/integer, ID, timestamp, category, native class and origin are unchanged by
the writer. Notify veto behavior remains available after a same-family change;
PROCESS-EVENT copies the final native veto state. Type values are process-local,
not stable serialized IDs. Saved snapshots remain readable after registry/session
cleanup and confer no native cast or object-lifetime authority.

Run the example, which registers both command tags and verifies changed-tag
parent delivery for synchronous and queued dispatch, then cleans up:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-tags.lisp --eval '(wxlambda-event-tags:start)'
```

Regression checks cover command/notify changes, built-in/session/cross-family and
unknown targets, bad integers, failed-write preservation, immutable snapshots,
nested/expired contexts and payload-preserving parent delivery. Public APIs cannot
manufacture an incompatible native object carrying a registered tag, so the
actual-class rejection branch is statically reviewed rather than directly
exercised. Tests do not impose local callback order or emit real OS session events.

SetEventType coverage remains partial. Native origin retention/SetEventObject,
generic wxObject wrappers, persistent native event ownership/cloning, arbitrary native userData adoption,
pushed handlers and broader event families remain C1 work. Next define the
ownership/lifetime policy needed for origin writes and persistent events before
exposing borrowed native object pointers or widening mutable tags.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [type/origin readers](EVENT-ORIGINS.md), [ID routing](EVENT-ROUTING.md) and
[docset installation](DOCSET.md).

[Owned events](OWNED-EVENTS.md) now provide detached native construction/cloning,
explicit disposal, mutable value fields and copied dispatch. Earlier immutable
snapshots and scoped contexts keep their lifetime rules. Arbitrary native classes,
origin retention and client/userData ownership remain open.
