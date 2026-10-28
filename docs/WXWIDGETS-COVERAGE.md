# wxWidgets coverage plan

The goal is **broad GUI coverage with selective depth**: useful, reliable Lisp
desktop applications rather than a literal wrapper for every wxWidgets API.
This supersedes the earlier exhaustive-parity objective. The pinned native
inventory remains factual: unbound/partial APIs stay visible even when deferred,
served by Lisp-native alternatives or unnecessary for a selected milestone.

C1–C7 are **inventory categories, not a delivery sequence**. Select feature work
from the priorities below and implement its actual prerequisites. Exhausting C1
is not required before everyday controls, menus, dialogs or navigation. Historical
checkpoint notes retain their former goal language, explicitly superseded here.

## Scope and inventory

Target: the published wxWidgets **3.2.12** API, the version recorded by the
project's existing local verification. CMake currently accepts 3.2 or newer;
this inventory does not claim to cover APIs added after 3.2.12. When advancing
the coverage target, regenerate and review the inventory delta explicitly.

The authoritative inventory is upstream's
[`v3.2.12/interface/wx`](https://github.com/wxWidgets/wxWidgets/tree/v3.2.12/interface/wx),
not just the `core` and `base` components currently linked by this bridge.
Extracted declarations retain the upstream [wxWindows Library Licence](coverage/UPSTREAM-LICENCE.txt)
and its [GNU Library GPL terms](coverage/UPSTREAM-LGPL.txt).
The checked-in [manifest](coverage/manifest.json) records every header's SHA-256,
phase and entry count. The [generated coverage summary](coverage/README.md) gives current implemented,
partial and pending counts by phase and header. The [class/widget index](coverage/classes.md) lists every
class/struct definition found there. Each C-phase links to the full source
checklist: methods, constructors, destructors, overloads, operators, free
functions, callback types, enums, constants, macros, templates, and documented
style/event table entries. Supporting source chunks are retained conservatively as context without
checkboxes; entry counts are **not counts of distinct functions**. No platform branch is
preprocessed away. Each entry links to its upstream source line.

This is a complete inventory of the pinned **interface headers**, not proof
that these documentation headers contain every public symbol in the full
wxWidgets source tree. Reconcile additional documented public APIs, generated declarations, port
headers and inherited interfaces when they affect selected features or any
claim of native completeness. Undocumented
implementation internals are not binding requirements. Deprecated public APIs
remain visible; any decision to omit an API must be recorded explicitly rather
than checked off as implemented. Selected platform/optional features need explicit build selection, feature
detection and unavailable-feature behavior; unselected ports/components are not
automatic release requirements.

## Status rules

- `[x]` in the baseline below means the **named, bounded operation** exists in the
  current native bridge, FFI and exported Lisp API. It does not mean the whole upstream
  class or every constructor option is supported, or that all platforms passed.
- In generated checklists, `[x]` means the declaration's operation is implemented
  with its stated Lisp mapping, `[~]` means partial support with the supported
  arguments/receivers and remaining limits stated inline, and `[ ]` means pending.
  The class index uses `[~]` for the managed `wxAppConsole`/`wxApp` interfaces,
  `wxEvtHandler`, ten concrete window families, managed menu resources,
  `wxBoxSizer`, `wxWindow` and `wxTopLevelWindow`; none has
  complete class coverage. Applications have lifecycle functions and a borrowed application-handler view;
  native application construction and subclass hooks remain unbound.
  Constructors with fixed IDs/styles remain partial.
- [status.json](coverage/status.json) is the persistent source of declaration and
  class statuses, Lisp exports, native ABI names and supported scopes. Exact
  header/line/signature matching prevents silently transferring a mark to the
  wrong overload. The generator rejects missing/stale or duplicate mappings,
  unknown Lisp/native names, and overlapping phase assignments.
- Conditional directives, includes, include guards and inline implementation
  statements are labeled **Context** without checkboxes. Public macro definitions
  and constants remain API tasks. Context is retained for platform interpretation
  and is counted separately in the manifest.
- The baseline is based on `native/wxlambda.{h,cpp}`, `lisp/{ffi,api,package}.lisp`
  and existing tests. Historical test results remain in the implementation plan;
  no GUI or hosted-platform checks were rerun for this documentation change.
- For each binding, record the Lisp name, native ABI entry, supported arguments,
  return values, receiver types, ownership, events, platform availability and
  focused verification before closing its full declaration. One mapping can
  implement several overloads when all semantics remain expressible.
- C++ conveniences may map to Lisp strings, numbers, sequences, conditions or
  scoped resource forms instead of literal C++ wrappers. Such equivalences
  require an explicit mapping, including Unicode, range and lifetime behavior.

## Native status and GUI milestone acceptance

Use the [generated phase/header backlog](coverage/README.md) for counts and the
linked C1–C7 checklists for exact declarations. `[ ]` means no public binding is
recorded; `[~]` means only the annotated scope exists. Remaining native scope
is scheduled by feature priority, not automatically required for a release. A class marked partial is not complete because its constructor
works. Context, class-definition and declaration counts must not be interpreted
as percentages of distinct wxWidgets APIs.

The current baseline has ten concrete window kinds, basic layout and a deeply tested
managed lifetime/event layer, including command/notify/close/idle families, owned
resources, pending delivery, handler chains/stacks and rooted Lisp associations.
This foundation supports breadth; it is not a mandate to keep draining C1.
Menus/actions deliver the first part of the selected form/editor slice.
Modal text-entry/single-choice dialogs add bounded input workflows; message/file
families require a safe platform cancellation bridge.
Native notify subclasses, retained origins and other unselected depth stay
visible as native gaps without automatically blocking that GUI work.
See the [C1 details](coverage/phase-c1.md), [C2 details](coverage/phase-c2.md) and
[class index](coverage/classes.md) for the remaining work within bound families.

The scheduled-deletion ancestor gap is repaired: the bridge uses public wxApp
scheduled-deletion state plus its pending markers. Native regressions, independent
review and the full GUI/package checks below validate the corrected behavior.

The current bridge is ABI 33, with 158 public Lisp exports. The standard-dialog
batch passes all 50 GUI scenarios on local Apple Silicon macOS SBCL and CCL, five
package checks, independent reviews and the zero-assert native regression.
Reports: `evidence/standard-dialogs/{sbcl,ccl}-20261028-final/` and
`evidence/packaging/standard-dialogs-20261028-final/`. Docset checks pass with 221 entries
and zero problems, including actual public-export/class/SETF coverage and isolated
discovery/lookup. These checks do not validate hosted Windows/Linux, physical
interactive input, real OS session delivery or the graphical Cadre viewer.

A milestone is accepted against its selected GUI scope and representative
workflows, not declaration totals or completion of every pending native entry.

### Feature policy and priorities

| Policy | Selection rule |
| --- | --- |
| Core | Reliable ownership/invalidation, events/callbacks, main-thread rules, CLOS construction/SETF, and documentation/examples/tests for every delivered API. Keep basic windows/controls/layout reliable. |
| Planned | Everyday controls and layouts, menus/actions/toolbars/status bars, standard dialogs, navigation/notebooks, list/tree views, timers/idle, clipboard, drawing and necessary value adapters. |
| On-demand | Specialized grid/data-view models, printing/document frameworks, AUI/ribbon, rich text/STC, web/media/OpenGL and platform integration when an application needs them. |
| Lisp-native | Prefer Lisp strings, sequences, pathnames, hash tables, conditions, streams/configuration and utility libraries where literal native wrappers add no GUI value; document semantic differences. |
| Deferred | Unused overloads/styles, deprecated/duplicate APIs, arbitrary pointer adoption, broad native virtual overrides and optional components without a current consumer. Revisit with demonstrated demand. |

These are feature-level priorities, not native checkbox states. Do not bulk-mark
status.json to represent deferral or assume a Lisp equivalent without checking
its required semantics. A class can stay native-partial while its selected GUI
feature scope is accepted. Native missing APIs are not automatic release blockers.

**Current vertical slice:** menus/actions and bounded modal text/choice dialogs for an
everyday form/editor, adding only timer/value/lifetime prerequisites actually
needed by that slice. Do not drain native ownership/utilities first.

### Representative application acceptance

- **Form/editor:** common controls and validation, flexible layouts, menu/action
  and toolbar/status feedback, standard open/save/message dialogs, clipboard and
  keyboard/navigation basics. Existing small form examples are a foundation,
  not a claim that this complete workflow already exists.
- **Tree/list manager:** useful tree/list selection and item/model adapters,
  contextual/menu actions, navigation, dialogs and clipboard. Pick model depth
  needed by the application before literal native container/document parity.
- **Drawing/monitoring tool:** canvas, bitmap/text/geometry/font/colour adapters,
  practical resize/DPI behavior and bounded timer/idle updates. Every renderer or
  print backend is not a prerequisite for a usable first drawing workflow.

For selected feature scopes, verify end-to-end examples, Unicode/bounds,
callback errors, destruction/cleanup and packaging where applicable. Maintain
complete public-export reference coverage and zero docset checker problems.
Automated reader lookup remains separate from graphical Cadre verification.

Cross-platform checks stay explicit: local Apple Silicon macOS SBCL/CCL results
are recorded per batch; Linux/Windows Lisp/GUI/packaging work remains open until
actual reports pass. A milestone may be scoped to verified targets, but must not
advertise untested platforms or infer native parity from one backend. Record
unavailable runners and feature gates rather than treating every port as complete.

All entries here are checked only for the behavior stated. None of the eight
widget classes has complete wxWidgets coverage yet.

### C1: Application and bridge

- [x] Native loading: `load-native-library`; explicit-path packaged loading.
- [x] Application initialization/cleanup: `call-with-application`, `with-application`.
- [x] Main loop and stopping: `run`, `stop`.
- [x] Nonblocking event polling: `process-events`.
- [x] Active-application main-loop/queued-event state: `main-loop-running-p`,
  `pending-events-p`; explicit wx pending dispatch: `process-pending-events`.
  These do not complete arbitrary wxApp/event-loop bindings. All dispatch entries
  reject pump/callback reentrancy; see [application events](APPLICATION-EVENTS.md).
- [x] Interactive application loop and debugger return: `with-repl-application`,
  `return-to-application-repl` (SBCL/CCL; one GUI session per process).
- [x] Native-backed standalone `event-handler` ownership, borrowed window base
  identities, `handler-alive-p`, `handler-enabled-p` and its generic SETF writer,
  plus `destroy-event-handler` for standalone objects. Weak window tracking
  prevents stale pointer access when destroy-event delivery is disabled.
  Arbitrary event types and pushed-handler ownership remain open; see
  [event-handler scope](EVENT-HANDLERS.md).
- [x] Independent `bind-event`/`unbind-event` and `binding-active-p`, with
  four-kind command observers, exact IDs/ranges, safe self/peer removal, and
  `queue-event` on standalone/window handlers. `event-id` is a copied native ID;
  `event-source` is the registered receiver, including on parent observers.
  Observers Skip automatically; optional contextual bindings now explicitly
  control Skip and propagation. Arbitrary event families and veto remain open; see
  [supported binding scope](EVENT-BINDINGS.md).
- [x] Borrowed `event-context` values from `bind-event :with-context t`, with
  `event-skipped-p` SETF, `event-propagates-p`, stop/resume actions, nested callback
  isolation and stale-context checks. `event-native-type` copies runtime IDs;
  `event-type` looks up four supported tags. See
  [scoped context limits](EVENT-CONTEXTS.md); close-veto is now scoped and CanVeto-guarded.
- [x] `:close` / wxCloseEvent family, immutable `event-family` and close flags,
  scoped `event-can-veto-p` / `event-vetoed-p` with generic SETF, copied close
  queues and frame-only `close-window` requests. `event-type` replaces the old
  command-only lookup. Notify/session events and arbitrary families remain open;
  see [close-event limits](CLOSE-EVENTS.md).
- [x] `register-event-type` for unique session command/notify names, copied
  `event-integer`/text metadata, scoped notify Veto/Allow and `process-event`
  with native handled plus final-veto results. No unregister or type-declaration
  rollback; session-event routing remains open. See [notify limits](NOTIFY-EVENTS.md).
- [x] Borrowed `application-handler` identity via `current-application-handler`, with inherited binding/dispatch/state and rejected construction/destruction. Native application hooks and session routing remain open.
- [x] Query/end-session tags, current/saved `event-logging-off-p`, scoped SETF, application bindings and isolated owned-handler synthetic queues/dispatch. Real OS delivery and native shutdown policy validation remain open.
- [x] Copied/current event timestamp and native command classification, bounded native LONG timestamp SETF and close-family CanVeto SETF. Snapshot immutability and scoped lifetime remain enforced.
- [x] Current event-ID reads and SETF with signed bounds, unchanged snapshots and verified later parent exact/range/wildcard matching.
- [x] Current native type reads and captured/current opaque origin IDs for registered bridge objects. Native origin/type writers, general wxObject wrappers and ownership remain open.
- [x] Saved/current event category and compiled :UI/:USER-INPUT/:SOCKET/:TIMER/:THREAD/:ALL mask lookup. ALL includes actual native bits; category-specific families and YieldFor remain open.
- [x] Registered same-family event-native-type SETF with actual native class checks; built-in/session/unknown tags reject. Origin retention and arbitrary tag mutation remain open.
- [x] OWNED-EVENT construction/cloning/disposal/liveness, generic command text/integer SETF and copy-based native dispatch. Exact native classes and detached origins only; arbitrary classes, retained origin, source transfer and client/userData ownership remain open.
- [x] GUI POST-EVENT copies, consuming TRANSFER-EVENT handoff, PROCESS-EVENT-LOCALLY copies and non-application DELETE-PENDING-EVENTS with pending-list synchronization. Worker posting, app-wide deletion and per-handler pending processing remain open.
- [x] Standalone handler NEXT/PREVIOUS peer readers and reciprocal SETF, HANDLER-UNLINKED-P and UNLINK-EVENT-HANDLER. Strong canonical roots preserve wrapper identity; dispatch-time edits and borrowed window/app chains reject. Native Push/Pop/Remove remain open.
- [x] Managed window Get/Push/Pop/RemoveEventHandler and explicit ProcessWindowEvent/Locally copies. Push preserves bridge ownership; actual native window destruction detaches handlers. External stack adoption/SetEventHandler remain open.
- [x] Native extra LONG and raw IsSelection metadata, kind-specific live checked/selection SETF and clear-only compatible registered origin detachment. Retained/new origins and full derived-class payload semantics remain open.
- [x] Managed EVENT-HANDLER-CLIENT-DATA EQ Lisp storage through typed native root-token adapters, with foreign typed/untyped rejection. Generic wxClientData/container, untyped/event/item/userData ownership remain open.
- [x] BIND-EVENT :USER-DATA and read-only EVENT-USER-DATA saved/current association access through native wxObject token adapters. Active self-unbind and snapshot lifetime are retained; arbitrary native userData/client/item pointers and association mutation remain open.
- [x] :IDLE snapshots/current/owned resources, RequestMore, session-restored IDLE-MODE, own-window idle opt-in and GUI-thread wake scheduling. Explicit synthetic delivery is distinct from automatic policy; this does not imply full native idle/worker parity.
- [x] Native errors translated to Lisp `native-error` conditions.
- [x] Event snapshots: `event`, `event-p`, `event-kind`, `event-source`,
  `event-text`, `event-checked-p`, `event-selection` and family-specific payloads
  across supported command/notify/close/idle families; :MENU joins five command kinds.

### C2: Windows, controls and layout

- [x] `wxFrame`: `make-frame` with title, width and height.
- [x] `wxPanel`: `make-panel` with parent.
- [x] `wxButton`: `make-button` with parent and label.
- [x] `wxTextCtrl`: `make-text` with parent and initial value.
- [x] `wxStaticText`: `make-label` with parent and label.
- [x] `wxCheckBox`: `make-checkbox` with parent, label and initial boolean value.
- [x] `wxChoice`: `make-choice` with copied item strings and initial selection.
- [x] Static-text label get/set: `label-text`, `(setf label-text)` (not all controls).
- [x] Text value get/set: `text`, `(setf text)`; silent `ChangeValue` versus
  notifying `SetValue` for `wxTextCtrl`.
- [x] Checkbox boolean get/set: `checked-p`, `(setf checked-p)`, optional notification.
- [x] Progress gauges: native-backed `gauge` construction, `make-gauge`,
  `gauge-value`, `gauge-range`, `gauge-vertical-p` and `pulse-gauge`.
  Both numeric properties have generic SETF writers; range reductions clamp progress.
- [x] Choice index get/set: `selection`, `(setf selection)`, optional notification;
  Lisp NIL means no selection. Item mutation and three-state checkboxes remain open.
- [x] Parent-owned horizontal/vertical `wxBoxSizer`: `make-row`, `make-column`.
- [x] Add window children: `add-child` with proportion, uniform border, expand and
  minor-axis alignment; nested standalone sizers and full flags remain open.
- [x] Fixed/stretch spacing: `add-spacer`, `add-stretch`.
- [x] Relayout and geometry queries: `layout-window`, `window-position`, `window-size`.
- [x] Logical name get/set: `window-name`, `(setf window-name)`, with copied Unicode
  strings and explicit byte lengths preserving empty and embedded-NUL names.
- [x] Frame title get/set: `frame-title`, `(setf frame-title)`; other top-level
  receiver types remain unbound and their declarations are marked partial.
- [x] Show/hide: `show-window` accepts an optional boolean and returns the window
  plus the native changed-state boolean; `shown-p` queries its own shown flag.
- [x] Enable/disable input: `enable-window` returns the window plus changed-state
  boolean; `enabled-p` includes disabled ancestors.
- [x] Destruction requests: `destroy-window`; destruction notifications remain open.
- [x] Button bind/unbind/post: `on-click`, `off-click`, `queue-click`.
- [x] Text-change bind/unbind: `on-text-change`, `off-text-change`.
- [x] Checkbox toggle bind/unbind: `on-toggle`, `off-toggle`.
- [x] Choice selection bind/unbind: `on-selection`, `off-selection`.
- [x] Handler replacement and callback condition containment for those events;
  explicit unbinding of control-specific callbacks before window destruction is
  still required; generic registrations use weak invalidation and boundary pruning.

No APIs in C3–C7 currently have public bindings. Using wxWidgets types internally
for conversions or implementation is not public API coverage.

The widget-facing Lisp API has nine window wrapper classes plus the public event-handler base, direct native
construction and generic operations/SETF; see [CLOS API](CLOS-API.md). This establishes Lisp dispatch
and method combination, not native C++ subclass/virtual-callback coverage.

## Native inventory categories and feature selection

All 398 pinned headers remain in these source-linked categories. This is a
native discovery/status index, not delivery order or a promise to bind every
entry. Work across categories when selected GUI features need them; implement
necessary value/lifetime prerequisites without wrapping every utility first.

### C1: Foundation, values, application, events, and base utilities

[Complete C1 source inventory](coverage/phase-c1.md). Core event/lifetime/main-thread
contracts support delivered features. Idle/timers and GUI value adapters are
planned; filesystem/network/regex/container utilities normally use Lisp-native
libraries unless native integration needs a bridge. Broad pointer adoption and
virtual overrides remain on-demand/deferred.

### C2: Windows, layout, basic controls, and input

[Complete C2 source inventory](coverage/phase-c2.md). Plan useful radio/toggle/
combo/list controls, sliders/spinners, scrolling, layouts, focus/keyboard and
practical font/colour/DPI support. Deepen the existing window/control families as workflows
need; not every constructor option or inherited overload must precede the next
useful control.

### C3: Dialogs, navigation, menus, and application services

[Complete C3 source inventory](coverage/phase-c3.md). Menus/actions, toolbar/status
feedback, standard dialogs, books/notebooks and clipboard are everyday priorities.
Drag/drop and specialized platform/help/wizard services follow concrete demand.
Frame-owned normal/check/separator menus and ID-routed actions are implemented
within their verified scope. Modal text-entry/single-choice dialogs extend the
selected workflow, with final checks pending. Message/file cancellation is deferred;
popup/submenu/radio and full native menu overload depth stay in the inventory.

### C4: Models, complex controls, and document frameworks

[Complete C4 source inventory](coverage/phase-c4.md). Prioritize practical list/tree
views and Lisp item/model adapters for the manager workflow. Full grid/data-view,
native document/undo and persistence frameworks are on-demand; Lisp-native models
may fit better than literal native containers.

### C5: Drawing, images, graphics, and printing

[Complete C5 source inventory](coverage/phase-c5.md). Plan practical canvas/drawing,
bitmap/image, geometry, font and colour support. Choose useful rendering paths
first; every DC/backend, metafile or print integration is on-demand.

### C6: Optional libraries, rich content, and integration

[Complete C6 source inventory](coverage/phase-c6.md). Select AUI/ribbon/property-grid,
HTML/rich text/STC, web/media/OpenGL or XML/XRC when an application needs them.
Chosen components require explicit build/availability behavior; unselected optional
libraries do not block an everyday-GUI milestone.

### C7: Platform extensions and portability reconciliation

[Complete C7 source inventory](coverage/phase-c7.md). Verify platforms/backends
claimed by selected features and reconcile interface/implementation discrepancies
that affect them. Native Windows embedding/DDE/power and other port extensions
are on-demand. Preserve unbound facts without requiring every platform declaration
before a core GUI release or claiming unsupported native completeness.

## Completion and maintenance

Focused generator/inventory verification (using the same extracted sources):

```sh
python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface
```

Each selected feature scope closes when its chosen bindings have public Lisp
exports, native/FFI mappings and matching documentation,
required runnable examples, docset updates, and focused positive/error/lifetime/
event checks. Follow the [project implementation rule](../AGENTS.md) for every
implementation change: build the Cadre-format version 1 docset in `docs/docset/`
(`manifest.sexp`, `index.sexp`, `entries.sexp`), containing the indexed reference
for every implemented public wxlambda export, project guides and explained
example sources together. Run the relevant examples, require zero problems from
`docset:check-docset`, and verify export coverage, links and Cadre discovery,
symbol lookup and guide/example navigation. The docset is the primary
documentation deliverable. The initial Phase 7 build prerequisite is implemented
in [the docset tooling](DOCSET.md); maintain the generated artifact with each
implementation change. Platform-only work must be verified on its supported
platform before advertising that support; local macOS success cannot close
Linux/Windows verification work. Existing Phase 3
hosted checks remain pending.

To reproduce the baseline inventory, extract the pinned release's `interface/`
directory and run:

```sh
python3 tools/coverage_inventory.py /path/to/wxWidgets-3.2.12/interface
```

The generator is documentation-only and does not require an installed wxWidgets
build. Multiline preprocessor directives are kept together so their parentheses
do not swallow following classes or function declarations. It preserves source declarations rather than attempting C++ semantic
resolution; declaration fragments and helper tokens intentionally remain visible.
Compare the resulting header hashes with the committed manifest when verifying
the same source version. Update `docs/coverage/status.json` when implementing
bindings; regeneration preserves those marks in the function lists and class
index. Do not edit generated checkboxes directly. A new upstream version requires
reviewing and migrating the exact declaration mappings before regeneration.

Native pending-list synchronization uses an implementation-header management
method absent from the pinned documentation interface and remains a C7
reconciliation dependency; target cancellation does not close app-wide deletion.

Paired NIL standalone edge-detach semantics follow native implementation/Unlink
behavior although the pinned setter prose forbids NULL; retain that discrepancy
for C7 reconciliation.

Native window teardown requires pushed handlers to be removed; managed native
widget destructors pop without deleting bridge-owned handlers before base teardown.

Raw IsSelection reports extra-LONG action metadata, not event kind or choice-index
validity; origin mutation remains NIL-only and never retains a native object.

Protected wxEvtHandler storage-mode inspection avoids typed/untyped native
assertions; it remains an implementation-header C7 dependency, not a public
field/mode API.

The ABI 31 idle batch passes all 48 local GUI scenarios on both Lisps, five
package checks and the zero-assert native regression; independent reviews pass. Automatic idle policy is separate from explicit synthetic
delivery and own-window opt-in is not an effective eligibility predicate.

The verified ABI 32 menu batch adds managed native menu hierarchies and :MENU
commands; [MENUS.md](MENUS.md) defines its selected scope and completed checks. Bounded modal text/choice dialogs extend that scope; native menu gaps do not automatically block
that GUI feature.

The ABI33 modal base/text-entry/single-choice batch passes its documented
local suites, native regression, package and docset/export checks. [STANDARD-DIALOGS.md](STANDARD-DIALOGS.md) distinguishes raw modal results
from native validation/data transfer and stored-value/cache timing. Message/file
dialogs remain deferred after a macOS cancellation probe hang; arbitrary native
modal families are not claimed. Continue selected GUI features without requiring
all native ownership/event declarations first.
