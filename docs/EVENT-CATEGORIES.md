# Event-category metadata

ABI 28 adds EVENT-CATEGORY for saved immutable snapshots and current innermost
contexts, plus EVENT-CATEGORY-MASK for compiled native category values. The
coverage baseline is wxWidgets 3.2.12. Categories are numeric flags, not native
type IDs, EVENT-ID routing values or EVENT-FAMILY keywords.

EVENT-CATEGORY reads/copies virtual GetEventCategory. Current supported command
and notify-derived events report USER_INPUT; close/query-end/end-session events
report UI. This follows the native class override even for registered synthetic
command/notify tags; a synthetic event is not proof of actual user input.
All generic and control-specific snapshots capture native category at callback
entry and remain readable after native receiver/session cleanup.

EVENT-CATEGORY-MASK accepts :UI, :USER-INPUT, :SOCKET, :TIMER, :THREAD or :ALL.
It loads the bridge and reads compiled native constants without creating an
application or requiring an active GUI session. Native library/ABI loading can
fail normally. Unknown keywords signal a Lisp error before native lookup.
The five public bits in the pinned baseline are 1,2,4,8,16; use the lookup rather
than hardcoding them. Combine flags with LOGIOR and test membership with LOGTEST.

:ALL is the actual compiled wxEVT_CATEGORY_ALL mask, including any internal bits.
It is not necessarily the OR of only the five public category values. The pinned
interface documentation shows 31, but the local 3.2.12 implementation header also
includes internal UNKNOWN/CLIPBOARD bits and reports 127. Those internal names
are not public bindings here. Likewise, the documentation interface omits the
wxCommandEvent GetEventCategory override present in the implementation header.
These discrepancies are retained as evidence for C7 public-interface
reconciliation; they do not authorize binding undocumented implementation APIs.

```lisp
(logtest (wxlambda:event-category-mask :user-input)
         (wxlambda:event-category event))
```

No SETF writer is supplied: category is native virtual behavior, not a mutable
base event field. No native override/subclass callback is introduced. Context
reads require an active main-thread session and the innermost callback;
expired or suspended outer contexts signal NATIVE-ERROR. Saved snapshots need
no native session. Mask lookup is session-independent and does not pump events.

Run the isolated synthetic example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-categories.lisp --eval '(wxlambda-event-categories:start)'
```

Tests cover all seven built-in and registered command/notify kinds, each of the
four convenience snapshot paths, saved values, nested/expired context isolation,
invalid mask names and public-bit inclusion in the native ALL mask. No native
socket/timer/thread events or real OS session events are generated. Category
constants do not imply those event families are bound.

wxEventLoopBase::YieldFor and category-selective pumping remain unbound. Existing
PROCESS-EVENTS/PROCESS-PENDING-EVENTS behavior is unchanged. Remaining C1 work
includes typed tag mutation, origin retention, persistent event ownership/cloning,
userData, pushed handlers, timer/idle and worker dispatch. Next design bounded
registered-tag mutation using actual native class checks, retaining the session
fallback isolation and rejecting incompatible native tag changes.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre lookup and examples.
See [metadata](EVENT-METADATA.md), [origin/type readers](EVENT-ORIGINS.md) and
[docset installation](DOCSET.md).

[Registered-tag mutation](EVENT-TAGS.md) now supports same-family SETF with actual native class checks; built-in/session tags and origin writes remain excluded.
