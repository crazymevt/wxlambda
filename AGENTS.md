# Project instructions

## Long-term goal and session continuity

The goal is broad GUI coverage with selective depth, not exhaustive wxWidgets API
parity. Prioritize reliable lifetimes/events, main-thread rules, Lisp ergonomics,
documentation and tests while expanding everyday GUI features. C1–C7 are native
inventory categories, not a delivery sequence: implement prerequisites needed by
the selected feature, without exhausting C1 before controls, menus or dialogs.
Use the feature policy in docs/WXWIDGETS-COVERAGE.md: core, planned, on-demand,
Lisp-native and deferred. Native pending/partial statuses stay factual and are not
automatic release blockers. Do not mark an unimplemented native API complete to
represent a priority or intentional deferral. The earlier full-parity goal is
superseded; historical checkpoints retain their original context.

Read the session handoff in `IMPLEMENTATION.md` and `docs/WXWIDGETS-COVERAGE.md`
when resuming. Keep the handoff current with completed slices, the next dependency,
review findings, verification evidence and limitations. Use independent code
review, resolve actionable findings, then commit verified slices.

With each binding change, update `docs/coverage/status.json` and regenerate the
summary, class index and all C1–C7 checklists with `tools/coverage_inventory.py`.
The generated `docs/coverage/README.md` is the current phase/header backlog;
do not hand-edit generated counts or checkboxes. Check regeneration against the
pinned upstream sources and rebuild/check the docset, including that summary.

## Implementation deliverables

Every implementation change must include corresponding documentation, examples,
and docset updates. Keep all three aligned with the implemented behavior and
its supported scope; examples are required, not optional.

The docset is the primary documentation deliverable. Use the format consumed by
Cadre: the `docset` Common Lisp library's **format version 1**, specified in
[docset/docs/format.md](https://github.com/crazymevt/docset/blob/481f57e7222925c3157e41a7e3deaeb628ecc9a7/docs/format.md).
Build it in `docs/docset/` as three UTF-8 Lisp-data files: `manifest.sexp`,
`index.sexp`, and `entries.sexp`. Use docset ID `wxlambda` and package
`WXLAMBDA`, with a table of contents and indexed entries readable by Cadre.

Include these three content groups together in that docset:

- **Standard library reference content:** indexed entries for every implemented
  public wxlambda export, including callable APIs, event types/accessors and
  conditions. Document syntax, arguments, return values, behavior, errors and
  supported scope as applicable; identify related wxWidgets APIs and versions.
- **Project documentation:** installation, usage, development/debugging,
  packaging guides and documented limitations.
- **Project examples:** the example source code, explanations and commands to
  run it, accessible from the docset's index and table of contents.

Generate these as Cadre-readable entries, including code blocks for examples.
Keep the standalone documentation and example sources available as well.

Before marking implementation work complete, verify the documentation matches
its public interfaces, run the relevant examples, and build and verify the
docset includes all three content groups. Run `docset:check-docset` on the
output and require zero reported problems; also verify public-export coverage,
links, and Cadre discovery, symbol lookup and access to guides/examples.
Document how to install it for Cadre (for example, under
`$XDG_DATA_HOME/docsets/wxlambda/`, defaulting to
`~/.local/share/docsets/wxlambda/`). Report the checks and any failures or checks
not run; do not claim unfinished deliverables are complete.

See `IMPLEMENTATION.md` and `docs/WXWIDGETS-COVERAGE.md` for the roadmap and
binding completion criteria. The initial Phase 7 build prerequisite is now
implemented: use `python3 tools/docset.py build`, `check`, and `install` as
documented in `docs/DOCSET.md`. Continue building and checking docset updates
for subsequent implementation work, including Phase 3 fixes and C1–C7 bindings.

## CLOS API

Use native-backed CLOS construction: concrete classes create their native widget
through inherited `initialize-instance` methods using initargs such as `:parent`,
`:label` and `:title`. Users must be able to directly instantiate subclasses with
slot initargs and `:default-initargs`; native properties are available in subclass
`:after` hooks. Keep `make-*` factories thin, with validated `:class` support and
forwarded initargs. Reject abstract/invalid construction clearly and clean up
native widgets and callback roots when managed initialization fails.

Use generic `setf` accessors as the canonical property-writing protocol.
Name mutable properties without a `get-` prefix (`text`, `label-text`,
`frame-title`, `window-name`) so reading and SETF writing use one natural name.
Window/control operations use generic functions specialized on public wrapper
classes. The project is being built from the ground up; backward compatibility
is not required. Do not add setter aliases or receiver fallbacks solely for
compatibility. Keep native handle/type checks and callback/lifetime protections.
Application lifecycle functions, keyword-only factories, macros and the FFI can
remain ordinary functions/macros. Event snapshots and binding identities remain
immutable structures. Default command callbacks observe and automatically Skip. Generic
`:with-context t` bindings receive a borrowed event-context for explicit Skip and
propagation control; it is valid only in its current innermost callback. Preserve
that lifetime isolation and immutable snapshots when extending native event families.
Close veto is scoped and guarded by current native CanVeto. Context SETF CanVeto
maps native metadata directly (including re-enabling); snapshots remain immutable
and metadata changes do not guarantee OS veto policy. Frame close acceptance is not
a destruction guarantee. The type lookup is `event-type`, with no old-name alias.

The native-backed `owned-event` resource supports detached command/notify/close/idle
construction, independent cloning/disposal and copied dispatch. Reject ancestry
combining it with `event-handler`/window resources before native allocation.
Direct clone construction uses source native state over payload defaults; factory
clone calls reject explicit payload overrides. No raw origin retention or
event client/userData ownership is exposed. Managed EVENT-HANDLER-CLIENT-DATA
retains Lisp values through typed numeric-token adapters/session roots; foreign
typed/untyped modes reject untouched. It is not event payload or per-item data. BIND-EVENT :USER-DATA separately
retains per-registration EQ values through native entry-owned token adapters.
Snapshots capture the reference, not mutable contents; active context frames
retain it across unbinding. No EVENT-USER-DATA SETF or raw pointer adoption. Origin SETF is clear-only NIL: non-null
current origins require registered compatible command/notify classes; built-in
non-null origins reject. Semantic checked/selection views remain kind-limited,
while raw IsSelection is extra-LONG action metadata independent of kind/index.

The native-backed `event-handler` base supports standalone session-owned handlers;
windows borrow their existing native widget identity. Managed standalone chain
links preserve reciprocal edges and canonical strongly rooted wrapper identity;
structural edits reject RUN/pump/callback use and linked callback-time destruction
rejects. Do not widen standalone peer operations to borrowed window/app tails or
attached nodes. Dedicated managed window Push/Pop/Remove APIs preserve bridge
session ownership; actual native window destruction pops managed stacks without
deleting those handlers. Explicit stack-aware processing differs from generic
window receiver dispatch, which retains native stack bypass. Public window wrapper
classes are `window` (abstract), `frame`, `panel`, `button`,
`text-control`, `label`, `checkbox`, `choice`, `gauge`, `notebook`, `list-box`, `tree-control`, `splitter-window`, `toolbar` and `status-bar` (plus the
DIALOG hierarchy described below). Lisp subclassing does not imply
native C++ subclassing or virtual-method callbacks; those need separate bridge
support. See `docs/CLOS-API.md`.

## Shell output

Always run noisy non-interactive shell commands (including installs, builds,
tests, linters, and verbose batch jobs) through
`/Users/jessie/.local/bin/quiet-run.sh -- <command> [args...]`.
For shell pipelines or compound commands, wrap the entire operation with
`/Users/jessie/.local/bin/quiet-run.sh -- bash -lc '<script>'`, quoting safely.
Use the wrapper's status and log path; inspect only relevant excerpts of the
full log when needed to diagnose failures or retrieve results. Preserve and
respect the wrapped command's exit code.
Short, targeted reads and interactive commands may run directly. If the wrapper
is missing or unusable, report that and use bounded output as a fallback.

Menu resources use the separate native-backed MENU-RESOURCE abstract hierarchy:
MENU-BAR, MENU and MENU-ITEM. Direct constructors use :PARENT; native frame/bar/
menu ownership controls invalidation. Structural menu edits reject callback/pump/
RUN contexts; scalar writes follow the documented generic protocols. See
`docs/MENUS.md`. Modal DIALOG has bounded TEXT-ENTRY-DIALOG/SINGLE-CHOICE-DIALOG families;
generic MESSAGE-DIALOG/FILE-DIALOG use an explicit guarded wx GUI backend;
platform-native message/file dialogs remain unbound. Preserve modal
context suspension and active-dialog/ancestor lifetime guards. See
`docs/STANDARD-DIALOGS.md` and `docs/MESSAGE-FILE-DIALOGS.md`. Preserve guarded
internal prompt depth/sequential cancellation; raw modal results do not imply
file validation or accepted selection.

The persistent form workflow in `examples/document-form.lisp` composes existing
menus/dialogs with Lisp-native UTF-8 document IO; it adds no native bindings.
Preserve cancellation/failed-save close veto, dirty baselines and native filename
conversion. See `docs/DOCUMENT-FORM.md`. ABI35 adds bounded text
clipboard support and form Edit Copy/Paste. Preserve isolated automatic transfer
tests: do not overwrite/read system clipboard content in CI; live availability is
the only automatic system query. See `docs/CLIPBOARD.md`. The splitter and toolbar/status-bar slices are implemented; see the current handoff
in IMPLEMENTATION.md for the next selected dependency. ABI36 multiline text/basic editing uses native
offsets, never Lisp string indices; preserve explicit UTF-8 lengths, NUL validation,
CLOS rollback and notifying Replace with callback/context protections. See
`docs/TEXT-EDITING.md`. Do not drain wxDocument/native stream APIs.

ABI37 notebooks own attached PANEL pages. Add pages only when quiescent outside
RUN/pumps/callbacks; no page self-attachment during initialization. Preserve
prepublication canonical wrapper rooting and rollback, actual/native propagated
page-callback lifetime guards, no same-book recursive selection and deferred modal/
clipboard entry. Native event selection/caption semantics vary by port; no synthetic
or owned notebook event support. See `docs/NOTEBOOK.md`.

ABI38 list boxes are unsorted single-selection text containers. Preserve silent
item mutations and deterministic selection adjustment, temporary event-blocker
handler-stack restoration, and actual list-origin generic/propagated/convenience
callback lifetime guards. Item edits/nested selection are allowed; destruction/
close and modal/clipboard entry defer until return. Simple list command events
remain constructible/cloneable and can target non-list handlers. See `docs/LIST-BOX.md`.

ABI39 tree items are borrowed canonical views backed by checked owner/item tokens,
not raw wxTreeItemId pointers. Preserve subtree/owner invalidation, early native-base
teardown suppression and revision-pruned session caches. Snapshot EVENT-TREE-ITEM
freezes view identity only; text/validity remain live. Nonempty selection needs a
same-tree item; normalize collapse/delete/root selection under blockers before idle.
Same-tree writes and native-loop entry defer during native/propagated selection
callbacks. No synthetic/owned/cloned tree events. See `docs/TREE-NAVIGATION.md`.

Keep wxQt tree construction unavailable until asynchronous raw-item events have
generation-safe translation; do not remove that gate based only on synchronous
macOS/GTK/MSW tests. See the ABI39 handoff in `IMPLEMENTATION.md`.

ABI41 frame toolbar/status bars use managed window lifetimes and quiescent
structure/deletion; fixed tools are addressed by toolbar-scoped positive IDs.
wxEVT_TOOL aliases MENU. Preserve actual/propagated origin lifetime/reentry guards,
app-owned status fields, and explicit safe in-window OSX toolbar construction.
Native Cocoa pointer-ID placeholder restoration is excluded. See docs/FRAME-ACTIONS.md.

ABI42 FRAME-ACCELERATORS stores copied normalized Lisp/native entry data; no
menu pointers. Preserve portable ID1..32767 and native-effective chord duplicate
checks, quiescent table writes, subtree registry teardown and conservative
all-MENU callback guards while explicit tables are installed. Native keyboard
commands vary by port in origin/checked/menu enabled behavior; shared actions
use the Lisp model. See docs/KEYBOARD-ACCELERATORS.md. wxUSE_ACCEL is a build prerequisite.

ABI43 deferred UI actions run only at a safe outer native idle boundary under
RUN/full polling. Preserve FIFO/one-per-idle, weak-owner/cancel/root/shutdown
semantics, pending-error pause, nonlocal-exit containment, native-entry TLS and
modal/operation/action-depth gates. The action body is a safe structural phase;
nested event callbacks retain guards, and nested RUN/pumps/finish must reject.
Never read/overwrite the system clipboard in automatic transfer tests; use the
in-memory native fixture. See docs/DEFERRED-UI-ACTIONS.md.

The document form now composes owner-bound deferred menu/tool/keyboard actions.
Preserve first-request-wins coalescing, delivery-time document/clipboard values,
separate pending/action-active/busy flags, cancellation on disposal/forced close,
and initial veto + deferred decision + scoped/consumed approval close replay.
Observe preceding vetoes without claiming arbitrary handler ordering. Keep form
action IDs in the portable accelerator range. See docs/DOCUMENT-FORM.md.

ABI44 UI timers use opaque tickets, private native wxTimer notifications and the
shared safe-idle UI phase. Preserve one due tick/coalescing, due/enqueue sequence
ordering, one-shot retirement-before-call, periodic self-cancel/error cancellation,
owner/session/root cleanup, count/interval bounds and no native pointer across
Lisp delivery. macOS start is gated to verified3.2.12: newer branches can lack the
queued-native cancellation validity check. Re-audit before widening this gate.
See docs/UI-TIMERS.md; no public wxTimer event receiver/Notify hook is exposed.
