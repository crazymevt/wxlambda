# Owned native events, detached clones and copied dispatch

ABI 31 adds the native-backed `owned-event` CLOS class and `make-event`,
`clone-event`, `destroy-event` and `event-alive-p`. These are independently owned
native resources, distinct from immutable `event` snapshots and borrowed
callback-scoped `event-context` values. The baseline is wxWidgets 3.2.12.

## Construction and disposal

`make-instance` on OWNED-EVENT or a subclass creates the native event through
inherited initialization. `make-event` is a thin factory with validated `:class`
and forwarded initargs. Subclass slots, `:default-initargs` and AFTER hooks work;
native properties are available in subclass AFTER hooks. Combining OWNED-EVENT
with EVENT-HANDLER or a window class is invalid and must reject before native
allocation. These wrappers do not install native C++ virtual overrides.

```lisp
(wxlambda:with-application ()
  (wxlambda:register-event-type :message :family :command)
  (let ((event (wxlambda:make-event :kind :message :text "Hello" :integer 1)))
    (unwind-protect
         (setf (wxlambda:event-text event) "Reusable payload"
               (wxlambda:event-integer event) 2)
      (wxlambda:destroy-event event))))
```

`:kind` defaults to :CLICK and accepts the nine constructible built-in kinds (not notebook page events) or a registered
command/notify name. The other base construction initargs match synthetic event
payloads: `:id` defaults to 0, `:text` to an empty string, `:integer` to 0,
`:checked-p` and `:selection` apply only to their built-in kinds, `:can-veto-p`
defaults T for close-family construction, initial `:vetoed-p` applies only to
registered notify kinds, and `:logging-off-p` applies only to session kinds.
Explicit wrong-family payload options follow QUEUE-EVENT validation. Initial ID
-1 remains reserved; subsequent EVENT-ID SETF can assign it as native metadata.
No native handle can be supplied as an initarg.

Native wxEVT_TEXT can lazily read the originating text control instead of its
stored command string. Context EVENT-TEXT SETF therefore rejects that type when
its tracked origin exposes a text entry, and rejects unknown native origins; it
does not silently edit the control. Detached context cloning first captures the
effective GetString value and stores it before clearing origin, freezing the
observed text. Copied owned :TEXT-CHANGE dispatch to a text-entry receiver rejects
before queueing or processing, rather than allowing receiver text to shadow the
owned payload. Other receivers and custom command tags support payload copying.
Existing keyword synthetic dispatch behavior is unchanged.

An owned event stays in the native session registry until explicit DESTROY-EVENT
or session cleanup, even if its Lisp wrapper is garbage collected. There is no
GC finalizer or cross-session reuse. EVENT-ALIVE-P returns NIL for an explicitly
destroyed resource during the active session. Other native access and repeated
destruction signal NATIVE-ERROR. A failed/uninitialized wrapper has no handle and
also signals NATIVE-ERROR, including for EVENT-ALIVE-P. Outside a GUI session,
all native resource operations signal NATIVE-ERROR. Native operations remain
restricted to the main thread.

Initialization failure rolls back native events, widgets, standalone handlers and
callback roots created in the same managed construction transaction. Window
rollback still follows native asynchronous destruction; event deletion is
immediate. Session cleanup destroys remaining owned events before handler/app
teardown.

## Detached native cloning

CLONE-EVENT accepts a live OWNED-EVENT or the current innermost EVENT-CONTEXT and
returns a new OWNED-EVENT. It calls native Clone only for exact wxCommandEvent,
wxNotifyEvent, wxCloseEvent or wxIdleEvent classes with supported tags. Other native subclasses
and events carrying borrowed native userData or command client pointers reject cloning. A saved
snapshot is not a native event and has no CLONE-EVENT method.

Native origin is cleared on every clone: a clone never retains a borrowed
GetEventObject pointer. EVENT-ORIGIN-ID on an owned event therefore returns NIL.
No new-origin setter, arbitrary wxObject wrapper or origin pinning/retention protocol
is added. Clone preserves supported value metadata, payload, Skip/propagation and
veto flags. Source and clone can be changed or destroyed independently.

CLONE-EVENT supports `:class` and forwarded subclass slot initargs. The factory
and MAKE-EVENT with `:clone-from` reject explicit payload override initargs: clone
first and use SETF to change its values. Inherited payload default initargs do
not replace cloned values. Direct MAKE-INSTANCE with `:clone-from` uses the source
native state and ignores payload initargs, including inherited defaults; subclass
slot initialization and AFTER hooks still apply. This source-wins rule avoids
accidentally reconstructing a clone from subclass defaults.

Cloning a suspended outer or expired context signals NATIVE-ERROR. Snapshot
readers stay independent of native lifetime; cloning does not extend a context's
lifetime or expose a native event pointer.

## Mutable properties

Owned events support generic reads for kind, family, ID, native type, timestamp,
command classification, category, CanVeto/veto, session logging-off, Skip and
propagation. SETF is supported for ID, timestamp, bounded registered native type,
CanVeto/veto, session logging-off and Skip. STOP-EVENT-PROPAGATION and
RESUME-EVENT-PROPAGATION also accept owned events. Kind/family/classification and
category are derived read-only metadata; origin remains detached.

EVENT-TEXT and EVENT-INTEGER are now generic readers for snapshots, current
contexts and owned events, with SETF for current/owned command and notify events.
Strings use explicit UTF-8 byte counts and retain embedded NULs. Integer writes
accept signed32 values. They alter native command payload fields, so built-in
checked/selection interpretations can reflect the same underlying integer.
EVENT-SELECTION snapshots normally contain an index, but explicitly mutated raw
integers can be other signed values; -1 maps to NIL. No live item-list validation
is implied by raw payload mutation.
Live text reads return a string for command/notify events, including empty
strings; close/idle-family text reads return NIL and writes reject. Close/idle-family
integer reads return zero and writes reject. Saved snapshots retain their
existing kind-specific copied payload interpretation and have no SETF methods.

Other validation is unchanged: timestamps must fit signed native C LONG; native
type mutation requires registered same-family tags and actual native class;
CanVeto writes require wxCloseEvent, veto writes check its current capability or
use notify Veto/Allow, and logging-off requires a session event. Context methods
remain valid only in the innermost callback. These metadata changes do not grant
OS veto authority or change the native class.

## Dispatch always copies

`(process-event receiver owned-event)` dispatches a fresh detached clone and
returns handled-p and final-vetoed-p from that copy. `(queue-event receiver
owned-event)` queues another independent native clone and returns RECEIVER.
Both reject synthetic payload keyword overrides; mutate the owned event before
copying instead. The receiver becomes the dispatch copy's native origin.

Mutations made by callbacks affect only that dispatch copy. The source owned
event remains unchanged and may be destroyed inside the callback or immediately
after queueing. Callback errors re-signal at the existing nearer Lisp boundary;
previous copy mutations are not transactional. The source is never transferred
to wxWidgets or consumed by dispatch. A contextual callback can CLONE-EVENT its
current native copy to retain the changed state, with origin detached again.
Observer Skip/handled-result and callback-time receiver-disposal rules remain in
force.

Session event copies still require an isolated owned standalone receiver.
Application/window receivers reject before dispatch, and standalone session
fallback to wxApp stays blocked. Owned session construction is not OS event
delivery and does not relax that rule. No live logout/shutdown is generated by
automated checks.

Run the complete example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/owned-events.lisp --eval '(wxlambda-owned-events:start)'
```

It constructs a reusable message, changes the synchronous copy, captures a
detached clone of the current context, queues a copy, destroys its source before
delivery, and verifies unchanged snapshots and independent payloads. Regressions
also cover CLOS defaults/subclasses, mixed-resource rollback, explicit disposal,
session cleanup, all built-in constructions, live payload writes and session
receiver guards.

This is partial native ownership coverage. Arbitrary derived event classes,
retained origin objects, client/userData ownership, snapshot-to-native conversion,
arbitrary native source transfer, origin writes and broader worker or
pushed-handler ownership remain open. Existing null/untracked-origin lookup and
actual-class mismatch rejection branches are not manufactured by the safe public
API. Hosted Windows/Linux GUI, native OS session delivery and graphical Cadre
validation remain unverified.

Build/check the docset with `python3 tools/docset.py build` and `check`; install
with `python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [event contexts](EVENT-CONTEXTS.md), [tag mutation](EVENT-TAGS.md),
[session events](SESSION-EVENTS.md) and [docset installation](DOCSET.md).

[Pending delivery modes](PENDING-EVENTS.md) now add GUI posting copies, genuine
TRANSFER-EVENT ownership handoff, synchronous local copies and handler-specific
pending deletion. PROCESS-EVENT/QUEUE-EVENT retain the copy semantics above;
transferred source wrappers are consumed and cannot be reused or destroyed.

[Standalone handler chains](HANDLER-CHAINS.md) now preserve canonical wrapper
identity and session ownership while participating in normal/local/pending
delivery. Structural edits and linked destruction during callbacks reject;
borrowed window/application stacks remain unbound.

[Managed window handler stacks](WINDOW-HANDLER-STACKS.md) now attach owned
standalone handlers without transferring native ownership, detach them on actual
window destruction and provide explicit stack-aware copy processing. Standalone
peer editing excludes attached nodes; arbitrary SetEventHandler/external adoption
remain unbound.

[Command payload views](COMMAND-EVENT-PAYLOADS.md) add native extra LONG, raw
selection-action metadata, uniform checked/selection semantic SETF and clear-only
origin detachment. Owned construction accepts command/notify :EXTRA-LONG; keyword
synthetic queue/process calls do not add that option.

[Handler Lisp client data](HANDLER-CLIENT-DATA.md) now retains EQ values through
a native typed token adapter with safe foreign-mode rejection. General native event/client/item pointer ownership remains unbound; per-binding
Lisp USER-DATA is separately supported.

[Per-binding Lisp user data](BINDING-USER-DATA.md) now provides BIND-EVENT
:USER-DATA and read-only snapshot/context association access, distinct from
handler client data and native command/client/item pointers. Borrowed native
userdata still prevents owned cloning; no mutable association API is added.

[Idle events](IDLE-EVENTS.md) add family :IDLE, bounded RequestMore, global
automatic policy and own-window opt-in metadata. Frame menus/actions now extend GUI breadth; standard dialogs are next,
without requiring exhaustive foundation parity.

[Menus and actions](MENUS.md) add built-in :MENU commands, including raw checked
metadata. Event field writes do not synchronize a managed menu item.
