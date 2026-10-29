# Scoped native event contexts

This C1 slice adds borrowed `event-context` values for native Skip and propagation
control. Existing `event` snapshots remain immutable Lisp-owned data. The bridge
is ABI 39, using the wxWidgets 3.2.12 inventory baseline. Supported events now include
seven command kinds (including :MENU and listbox selection/activation), the [close family](CLOSE-EVENTS.md), registered command/notify
types, [idle events](IDLE-EVENTS.md) and native [notebook page events](NOTEBOOK.md) and [tree selection events](TREE-NAVIGATION.md). Registered
[notify types](NOTIFY-EVENTS.md) support scoped Veto/Allow. Arbitrary native
classes and other native notify subclasses remain outside the bounded bridge;
platform session routing still needs target-specific verification.

## Opt into contextual callbacks

```lisp
(wxlambda:bind-event button :click
  (lambda (snapshot context)
    (format t "Command ~D; native type ~D~%"
            (wxlambda:event-id snapshot) (wxlambda:event-native-type snapshot))
    (setf (wxlambda:event-skipped-p context) t))
  :with-context t)
```

With `:with-context` true, the callback takes two arguments: an immutable snapshot
and a borrowed native-event context. **Contextual bindings begin unskipped**:
returning without setting Skip consumes the command at that handler. Callback
return values do not determine routing. Set `(event-skipped-p context)` true to
continue to later native handlers and normal parent routing; set it false to
consume. The generic writer returns the supplied generalized boolean, while the
reader normalizes the native flag to T/NIL.

The default `:with-context nil` remains an observer binding: its callback receives
only a snapshot and the bridge automatically Skips afterwards. Control-specific
`on-*` callbacks also remain observers. Callback order is still governed by
wxWidgets, with no Lisp ordering guarantee. A consuming contextual binding can
prevent later observers from running, but cannot undo an earlier delivery.
See [command binding and queue scope](EVENT-BINDINGS.md).

## Propagation is separate from Skip

```lisp
(let ((previous-level (wxlambda:stop-event-propagation context)))
  (assert (not (wxlambda:event-propagates-p context)))
  ;; Restore the level when parent routing is wanted again.
  (wxlambda:resume-event-propagation context previous-level)
  (setf (wxlambda:event-skipped-p context) t))
```

`event-propagates-p` maps to `wxEvent::ShouldPropagate`: it tests whether the
native propagation level is positive. `stop-event-propagation` sets that level
to zero and returns its previous value. `resume-event-propagation` restores a
nonnegative signed-32-bit level and returns the supplied context. Zero is allowed.
Invalid levels signal a Lisp type error.

Neither operation changes Skip. Stop plus Skip true allows later handlers on
the current receiver while preventing parent propagation. A positive propagation
level alone does not forward a consumed event. This exposes native command
propagation behavior, not a complete propagation/filter/handler-chain API.

## Exact borrowed lifetime

Contexts have private immutable tokens, not native pointers. They have no public
constructor and cannot be used to construct or clone native events. A native
scope registry validates both the token and callback depth before every access.
Only the **currently executing innermost native-to-Lisp callback** can use its
own context. A nested contextual callback suspends outer access. A nested observer
or control-specific callback also suspends outer access, even though it receives
no context of its own. After the nested callback returns, the outer context is
usable again until its own callback returns.

`event-context-p` only tests the Lisp value's type; it needs no session and remains
true for an expired value. `event-context-active-p` returns T for the currently
usable context and NIL for suspended/expired contexts in an active session.
Outside a session, the activity query signals `native-error`. All native readers,
writers and propagation actions reject suspended or expired contexts with
`native-error` rather than dereferencing an old event pointer.

The context expires on callback return, including after a callback condition.
Errors are contained and re-signaled using the existing native boundary; native
event mutations are not rolled back. Unbinding the current registration or
requesting deferred receiver disposal does not expire its still-running context:
the native event remains on the dispatch stack until return. Event-pump reentrancy
continues to be rejected. All operations require the active main-thread session;
worker-thread context access is not supported.

Keep the snapshot to inspect data after return. Do not keep a context expecting
to mutate the event later. The snapshot's source remains the binding receiver,
not necessarily the native originating child. General native origin-object wrapping and origin/type
mutation, arbitrary native userData adoption, arbitrary event classes and native virtual
overrides remain open. Veto is available for current vetoable close contexts and registered notify
contexts; plain command kinds do not provide veto state. See [close semantics](CLOSE-EVENTS.md).

## Native event type values

`event-native-type` reads a copied native `wxEventType` integer from any current
supported snapshot, including observer/control-specific callbacks:

```lisp
(= (wxlambda:event-native-type snapshot)
   (wxlambda:event-type :click))
```

The lookup accepts `:click`, `:text-change`, `:toggle`, `:selection`, `:menu`,
`:close`, `:query-end-session`, `:end-session` and `:idle`,
or a registered command/notify name, and requires an active main-thread session. Saved snapshot integers can be read without a
session. These are process-local runtime IDs; do not serialize them as stable
type identifiers or compare IDs from different processes. `event-kind` retains
the supported portable keywords. This is bounded native type-value introspection,
not custom type registration or arbitrary numeric-type binding. The old
`command-event-type` name was removed when the non-command close family was added.

## Example and checks

The [event-contexts example](../examples/event-contexts.lisp) uses a checkbox to
decide whether a contextual button callback forwards commands to its parent:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/event-contexts.lisp \
  --eval '(wxlambda-event-contexts:start)'
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/event-contexts.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/event-contexts.lisp
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. Its reusable `exercise`
runs the actual callback, checks forwarding/consumption, native type copying and
scope expiration, and cleans up bindings and windows. Focused tests additionally
cover stop/resume, suspended outer contexts across all three callback modes,
stale readers/writers, callback conditions, self-unbinding and deferred disposal.
The API, this guide and the example are included in the [Cadre docset](DOCSET.md),
installable under `$XDG_DATA_HOME/docsets/wxlambda/` (default
`~/.local/share/docsets/wxlambda/`). Hosted-platform GUI and graphical Cadre
verification remain separate checks.

[Session events](SESSION-EVENTS.md) extend the close family with query/end-session kinds, scoped logging-off metadata and isolated synthetic dispatch. Native OS session delivery remains unverified.

[Event metadata](EVENT-METADATA.md) adds copied/current timestamps and command classification, bounded timestamp SETF and close-family CanVeto SETF.

[Scoped event IDs](EVENT-ROUTING.md) now provide current ID reads/SETF and document effects on later matching. Snapshots remain immutable.

[Native origin/type readers](EVENT-ORIGINS.md) now distinguish captured origin identity from binding receiver and provide current native type access; origin/arbitrary-tag writers and general wxObject wrappers remain open.

[Event categories](EVENT-CATEGORIES.md) now provide saved/current native category flags and compiled keyword mask lookup, including the actual native ALL mask. Selective pumping remains unbound.

[Registered-tag mutation](EVENT-TAGS.md) now supports same-family SETF with actual native class checks; built-in/session tags and origin writes remain excluded.

[Owned events](OWNED-EVENTS.md) now provide detached native construction/cloning,
explicit disposal, mutable value fields and copied dispatch. Earlier immutable
snapshots and scoped contexts keep their lifetime rules. Arbitrary native classes,
origin retention and client/userData ownership remain open.

[Command payload views](COMMAND-EVENT-PAYLOADS.md) distinguish raw extra-LONG
selection-action metadata from kind-specific checked/selection views and add
bounded clear-only native origin detachment without retaining objects.

[Per-binding Lisp user data](BINDING-USER-DATA.md) now provides BIND-EVENT
:USER-DATA and read-only snapshot/context association access, distinct from
handler client data and native command/client/item pointers. Borrowed native
userdata still prevents owned cloning; no mutable association API is added.

ABI45 [numeric controls](NUMERIC-INPUT.md) add read-only EVENT-NUMERIC-VALUE for
slider/spin snapshots and current contexts. Spin updates are notify-derived but
observational; veto/raw payload changes do not roll back widget state. Numeric
event owned/synthetic/clone support is unbound; forwarded spin text callbacks
retain numeric lifetime/reentry guards.

`:radio-selection` command events expose copied/current index and native label; EVENT-SELECTION SETF and owned/synthetic/cloning reject. Shared input guards protect source/ancestors; see [radio groups](RADIO-GROUPS.md).

`:toggle-button-change` exposes copied/current checked metadata and native text (normally empty); semantic EVENT-CHECKED-P SETF and owned/synthetic/cloning reject. Source/ancestor Input guards apply to native and propagated callbacks. See [toggle buttons](TOGGLE-BUTTONS.md).

`:combo-selection` observes a native list index/text; semantic selection SETF and owned/synthetic/cloning reject for this kind. Existing :text-change remains shared command support. Both combo-origin families retain Input guards; physical selection may send COMBO then TEXT. See [combo boxes](COMBO-BOXES.md).

`:date-change` is a native command-family date-picker event. `event-date` reads a copied supported Gregorian tuple from the snapshot or current innermost context, independently of current control value; invalid/out-of-scope native event dates yield NIL. Non-date contexts and expired contexts reject. Date payload SETF, owned/synthetic date events and detached clones are unbound. Native/propagated/composite TEXT callbacks retain Input lifetime/reentry guards. See [date pickers](DATE-PICKER.md).

`:time-change` is a native command-family time-picker event. `event-time` reads a copied hour/minute/second tuple from the snapshot/current innermost context; midnight is `(0 0 0)`, invalid native payloads yield NIL. Non-time and expired contexts reject. Date payloads/timestamps remain separate; no semantic time writer, owned/synthetic time event or detached clone. Source/ancestor/composite Input guards and silent cross-picker synchronization apply. See [time pickers](TIME-PICKER.md).
