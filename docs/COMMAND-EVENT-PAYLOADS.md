# Command payload views and bounded origin detachment

ABI 28 adds EVENT-EXTRA-LONG and EVENT-SELECTION-P, extends EVENT-CHECKED-P and
EVENT-SELECTION to generic saved/current/owned readers with live SETF writers,
and adds clear-only SETF EVENT-ORIGIN-ID. The baseline is wxWidgets 3.2.12.
Snapshots remain immutable; current-context access remains innermost-callback
scoped and owned-event access remains valid only while the native resource lives.

## Extra LONG and raw selection-action metadata

EVENT-EXTRA-LONG reads the copied/current wxCommandEvent GetExtraLong value for
command/notify-derived events. SETF accepts signed native C LONG values and calls
SetExtraLong; native bounds are platform dependent (normally 32 bits on Windows,
64 bits on 64-bit Unix). Malformed/out-of-range writes reject before narrowing.
Close-family reads return zero and writes reject. This field is independent of
the signed32 EVENT-INTEGER payload and of a selection item's index.

MAKE-EVENT/MAKE-INSTANCE accepts optional `:extra-long` for command/notify
construction only. Clones preserve the source native value rather than inherited
payload defaults; clone factories reject explicit extra-long overrides just as
they reject other explicit payload overrides. Keyword synthetic QUEUE-EVENT and
PROCESS-EVENT do not add an :EXTRA-LONG option: construct or clone an owned event,
set its value, and dispatch it when that field is needed.

EVENT-SELECTION-P copies/reads raw native IsSelection for any command/notify
event: it is true when extra LONG is nonzero, and NIL when zero. Close-family
reads are NIL. It has no SETF writer; change EXTRA-LONG when raw action metadata
is intended. This is not an EVENT-KIND predicate, a test for :SELECTION or proof
that a choice index is valid. A normal choice event can have extra LONG zero and
therefore report NIL even though it has :SELECTION kind and a valid item index.
For listbox-like native events the flag can describe selection versus deselection,
but this batch does not bind listbox classes or reconstruct their item state.

## Semantic checked and selection views

EVENT-CHECKED-P retains the snapshot contract: only :TOGGLE reports checkbox-like
checked state; other supported kinds read NIL. Current/owned reads use the same
kind restriction. SETF accepts a generalized boolean only for a current/owned
:TOGGLE event, writes native SetInt as 0/1 and returns the supplied value. It
changes event payload metadata without calling a checkbox-control setter.

EVENT-SELECTION retains its kind restriction too: only :SELECTION returns the
native command selection integer, with -1 mapped to NIL; other kinds read NIL.
SETF is allowed only on current/owned :SELECTION events and accepts NIL (stored
as -1) or a nonnegative signed32 index. It validates no control item list and
does not call a choice-control setter or synthesize another native notification.

Raw EVENT-INTEGER still accepts any signed32 value for command/notify events.
Thus nonzero raw toggle values can read as checked, and raw selection mutation
can make GetSelection return another negative number. The semantic selection
reader preserves that native number; only -1 maps to NIL. Its SETF protocol is
stricter than raw integer access and rejects other negative indices. Raw
EVENT-SELECTION-P remains driven by EXTRA-LONG, not by this integer field.
Earlier snapshots retain their pre-callback values after any live mutation.
Later native or user handlers can react to changed metadata and change controls;
there is no guarantee that the whole dispatch leaves UI state unchanged.

```lisp
(let ((event (wxlambda:make-event :kind :selection :selection 0 :extra-long 0)))
  (unwind-protect
       (progn
         (assert (= 0 (wxlambda:event-selection event)))
         (assert (not (wxlambda:event-selection-p event)))
         (setf (wxlambda:event-extra-long event) 1)
         (assert (wxlambda:event-selection-p event))
         (setf (wxlambda:event-selection event) nil))
    (wxlambda:destroy-event event)))
```

## Clear-only native origin detachment

SETF EVENT-ORIGIN-ID accepts only NIL; non-NIL values signal TYPE-ERROR after
the active-session check. It never accepts a bridge integer ID,
wrapper or native pointer and never installs a new native origin. Owned events
are already detached, so NIL is an idempotent native-validated no-op while the
resource is live.

For a current context with an actual non-null native origin, clearing is allowed
only for a registered command/notify tag whose actual native class matches its
registered family. Built-in/nonregistered non-null origins reject before change.
An actual null origin permits a no-op. EVENT-ORIGIN-ID reader NIL can also mean
untracked, so reader NIL alone is not proof that this no-op condition holds.
No origin object is dereferenced, retained or destroyed by dropping the event's
pointer. This bounded policy avoids changing built-in semantics such as lazy
wxEVT_TEXT origin-backed strings; registered custom strings retain their payload.

EVENT-SOURCE remains the callback's Lisp registration receiver. Earlier snapshot
origin IDs remain unchanged; later callbacks can capture NIL after a successful
clear. Clearing does not change native class, type, ID, payload, ownership or
propagation policy. New copies dispatched from owned events still acquire their
receiver as origin, independently of the owned source's detached state.

All live reads/writes require the active main-thread session. Expired/suspended
contexts, destroyed/consumed owned events and unsupported field families reject.
Snapshot writers have no methods. There is no pointer retention/pinning protocol,
arbitrary SetEventObject, client/userData ownership or raw native adoption.

Run the complete example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/command-event-payloads.lisp --eval '(wxlambda-command-event-payloads:start)'
```

Validation should distinguish the raw action flag from semantic kind/index,
extra LONG bounds from signed32 payload bounds, detached no-op from guarded
context clearing, and immutable snapshots from current native state. Safe APIs
do not manufacture arbitrary native class mismatches or untracked origins;
those prior coverage limitations remain. Hosted Windows/Linux GUI, real OS
session delivery and graphical Cadre validation remain separate requirements.

Remaining C1 work includes retained arbitrary origins, full SetEventObject,
client/userData ownership, broader command-derived native classes and their
specific payload semantics, timer/idle and worker dispatch. Do not treat a native
extra field or generic flag reader as completion of every derived event class.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [owned events](OWNED-EVENTS.md), [origin/type access](EVENT-ORIGINS.md),
[metadata](EVENT-METADATA.md) and [docset installation](DOCSET.md).
