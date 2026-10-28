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
`text-control`, `label`, `checkbox`, `choice` and `gauge`. Lisp subclassing does not imply
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
