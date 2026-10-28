# wxWidgets coverage plan

This is the deliverable for implementation **Phase 6**. The former release
preparation phase is now **Phase 7** in [IMPLEMENTATION.md](../IMPLEMENTATION.md).
Creating this plan does not implement the remaining bindings or make exhaustive
coverage a prerequisite for the bounded first release. Coverage implementation
has its own phases **C1–C7** below, distinct from release roadmap numbering.

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
phase and entry count. The [class/widget index](coverage/classes.md) lists every
class/struct definition found there. Each C-phase links to the full source
checklist: methods, constructors, destructors, overloads, operators, free
functions, callback types, enums, constants, macros, templates, and documented
style/event table entries. Supporting source chunks are retained conservatively as context without
checkboxes; entry counts are **not counts of distinct functions**. No platform branch is
preprocessed away. Each entry links to its upstream source line.

This is a complete inventory of the pinned **interface headers**, not proof
that these documentation headers contain every public symbol in the full
wxWidgets source tree. The final reconciliation task below must resolve any
additional documented public API, generated declarations, port headers and
inherited interfaces before claiming full wxWidgets support. Undocumented
implementation internals are not binding requirements. Deprecated public APIs
remain visible; any decision to omit an API must be recorded explicitly rather
than checked off as implemented. APIs unavailable on a particular port remain
required on their supported ports. Optional components need selectable builds,
feature detection and clear unavailable-feature errors.

## Status rules

- `[x]` in the baseline below means the **named, bounded operation** exists in the
  current native bridge, FFI and exported Lisp API. It does not mean the whole upstream
  class or every constructor option is supported, or that all platforms passed.
- In generated checklists, `[x]` means the declaration's operation is implemented
  with its stated Lisp mapping, `[~]` means partial support with the supported
  arguments/receivers and remaining limits stated inline, and `[ ]` means pending.
  The class index uses `[~]` for the seven bound widgets and `wxBoxSizer`; none
  has complete class coverage. Constructors with fixed IDs/styles remain partial.
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

## Already implemented: bounded baseline

All entries here are checked only for the behavior stated. None of the seven
widget classes has complete wxWidgets coverage yet.

### C1: Application and bridge

- [x] Native loading: `load-native-library`; explicit-path packaged loading.
- [x] Application initialization/cleanup: `call-with-application`, `with-application`.
- [x] Main loop and stopping: `run`, `stop`.
- [x] Nonblocking event polling: `process-events`.
- [x] Interactive application loop and debugger return: `with-repl-application`,
  `return-to-application-repl` (SBCL/CCL; one GUI session per process).
- [x] Native errors translated to Lisp `native-error` conditions.
- [x] Event snapshots: `event`, `event-p`, `event-kind`, `event-source`,
  `event-text`, `event-checked-p`, `event-selection` for the four bound event kinds.

### C2: Windows, controls and layout

- [x] `wxFrame`: `make-frame` with title, width and height.
- [x] `wxPanel`: `make-panel` with parent.
- [x] `wxButton`: `make-button` with parent and label.
- [x] `wxTextCtrl`: `make-text` with parent and initial value.
- [x] `wxStaticText`: `make-label` with parent and label.
- [x] `wxCheckBox`: `make-checkbox` with parent, label and initial boolean value.
- [x] `wxChoice`: `make-choice` with copied item strings and initial selection.
- [x] Static-text label get/set: `get-label`, `set-label` (not all controls).
- [x] Text value get/set: `get-text`, `set-text`; silent `ChangeValue` versus
  notifying `SetValue` for `wxTextCtrl`.
- [x] Checkbox boolean get/set: `checked-p`, `set-checked`, optional notification.
- [x] Choice index get/set: `selection`, `set-selection`, optional notification;
  Lisp NIL means no selection. Item mutation and three-state checkboxes remain open.
- [x] Parent-owned horizontal/vertical `wxBoxSizer`: `make-row`, `make-column`.
- [x] Add window children: `add-child` with proportion, uniform border, expand and
  minor-axis alignment; nested standalone sizers and full flags remain open.
- [x] Fixed/stretch spacing: `add-spacer`, `add-stretch`.
- [x] Relayout and geometry queries: `layout-window`, `window-position`, `window-size`.
- [x] Default show and destruction: `show-window`, `destroy-window`;
  hide, general visibility options and destruction notifications remain open.
- [x] Button bind/unbind/post: `on-click`, `off-click`, `queue-click`.
- [x] Text-change bind/unbind: `on-text-change`, `off-text-change`.
- [x] Checkbox toggle bind/unbind: `on-toggle`, `off-toggle`.
- [x] Choice selection bind/unbind: `on-selection`, `off-selection`.
- [x] Handler replacement and callback condition containment for those events;
  handler unbinding before window destruction is still required.

No APIs in C3–C7 currently have public bindings. Using wxWidgets types internally
for conversions or implementation is not public API coverage.

## Coverage implementation phases

These phases group **all** headers in the manifest; the linked lists are the
individual widget/function work items, not examples of an unspecified larger set.
Within a phase, implement prerequisite value types and base classes before their
consumers. A cross-phase dependency can be pulled forward without marking the
entire dependent phase complete.

### C1: Foundation, values, application, events, and base utilities

[Full C1 declaration checklist](coverage/phase-c1.md)

- [ ] Complete application traits, initialization, event loops, generic
  `wxEvtHandler` binding/unbinding, event filters, event classes, propagation,
  veto/skip, idle processing, timers and thread-to-GUI dispatch.
- [ ] Complete object/type information, client data, strings/encodings, buffers,
  arrays/containers, variants/any, numeric/date/time types and conversions.
- [ ] Complete base utilities: files/directories/paths, streams, archives and
  compression, configuration, persistence prerequisites, logging/debugging,
  processes, dynamic libraries, environment/system utilities and command lines.
- [ ] Complete networking, protocols, web requests, IPC, threading/synchronization,
  translation/locales, regular expressions, security/secret storage and platform info.
- [ ] Define ownership, borrowed values, invalidation, subclass/virtual dispatch,
  callback lifetimes, thread restrictions and ABI compatibility for expanded APIs.

### C2: Windows, layout, basic controls, and input

[Full C2 declaration checklist](coverage/phase-c2.md)

- [ ] Finish existing seven widget classes, all constructor options and inherited APIs.
- [ ] Complete window/top-level/control bases: parenting, IDs, focus, capture,
  enable/show/hide, sizing/positioning, scrolling, styles, colours/fonts, DPI,
  tooltips, accessibility hooks and lifecycle notifications.
- [ ] Bind every remaining C2 widget in the class index: bitmap/command-link/toggle
  buttons, radio controls, combos, list/checklist boxes, sliders, gauges, spin
  controls, pickers, date/time/calendar, search, static controls and other controls.
- [ ] Complete all sizer families/items/flags, nested layouts, splitters,
  scrolled/virtual windows, validators, keyboard/mouse and control-specific events.

### C3: Dialogs, navigation, menus, and application services

[Full C3 declaration checklist](coverage/phase-c3.md)

- [ ] Complete modal/modeless and standard dialogs, custom file-dialog controls,
  wizards, preferences and help APIs.
- [ ] Complete menus/items, toolbars/status bars, books/notebooks, popups,
  mini/MDI windows, notifications/taskbar and splash windows.
- [ ] Complete clipboard/data formats, drag/drop, art/stock resources,
  display/settings, accessibility and UI action simulation.

### C4: Models, complex controls, and document frameworks

[Full C4 declaration checklist](coverage/phase-c4.md)

- [ ] Complete list/tree/tree-list/data-view/grid controls and all related models,
  renderers, editors, selection, sorting, virtual data and event classes.
- [ ] Complete headers/columns, image lists and item attributes.
- [ ] Complete document/view, document MDI, command/undo and persistent-object APIs.

### C5: Drawing, images, graphics, and printing

[Full C5 declaration checklist](coverage/phase-c5.md)

- [ ] Complete geometry/transforms, colours, pens/brushes/fonts, cursors/regions,
  bitmap bundles/icons/images, image handlers, animation and raw pixel access.
- [ ] Complete every DC/backend, graphics contexts/paths, overlays, native renderer,
  text metrics, paint events and buffered drawing with scoped resource ownership.
- [ ] Complete print/page setup/preview, print data and platform print integration.

### C6: Optional libraries, rich content, and integration

[Full C6 declaration checklist](coverage/phase-c6.md)

- [ ] Complete AUI docking, ribbons, property grids and related managers/events.
- [ ] Complete HTML, rich text and Styled Text Control, including editing models,
  commands, styles, printing and callbacks.
- [ ] Complete XML/XRC resource loading and custom handlers.
- [ ] Complete web views/handlers, media/sound, OpenGL contexts and rich tooltips.
- [ ] Extend component selection/builds beyond current `core`/`base`, and document
  native dependencies, backend availability and packaging for each optional library.

### C7: Platform extensions and remaining portability APIs

[Full C7 declaration checklist](coverage/phase-c7.md)

- [ ] Complete Windows-specific interface declarations, native-window embedding,
  DDE, dial-up, joystick and power APIs on their applicable platforms.
- [ ] Reconcile the interface-header inventory against upstream public headers,
  generated API documentation, event/style tables and build options; inventory
  and phase any additional public widgets/functions discovered before closure.
- [ ] Audit inherited APIs and all overloads, enum values, style combinations,
  virtual overrides, callback signatures and ownership across every class.
- [ ] Run binding, GUI, Unicode, callback/lifetime and packaged-application checks
  on supported OS/Lisp combinations; record feature-specific skips and reasons.
- [ ] Publish the supported API/feature matrix and resolve every remaining item
  as implemented, equivalent Lisp mapping, or explicitly unsupported. Unsupported
  required APIs prevent claiming full coverage; they are not completed bindings.

## Completion and maintenance

Focused generator/inventory verification (using the same extracted sources):

```sh
/Users/jessie/.local/bin/quiet-run.sh -- \
  python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface
```

Each phase closes only when its detailed inventory is reviewed and its required
bindings have public Lisp exports, native/FFI mappings, matching documentation,
required runnable examples, docset updates, and focused positive/error/lifetime/
event checks. Follow the [project implementation rule](../AGENTS.md) for every
implementation change: build the Cadre-format version 1 docset in `docs/docset/`
(`manifest.sexp`, `index.sexp`, `entries.sexp`), containing the indexed reference
for every implemented public wxlambda export, project guides and explained
example sources together. Run the relevant examples, require zero problems from
`docset:check-docset`, and verify export coverage, links and Cadre discovery,
symbol lookup and guide/example navigation. The docset is the primary
documentation deliverable. Its initial build tooling is the first prerequisite
in implementation Phase 7, before subsequent implementation work. Platform-only work must be verified on its supported
platform; local macOS success cannot close Linux/Windows work. Existing Phase 3
hosted checks remain pending.

To reproduce the baseline inventory, extract the pinned release's `interface/`
directory and run through the project's shell-output wrapper:

```sh
/Users/jessie/.local/bin/quiet-run.sh -- \
  python3 tools/coverage_inventory.py /path/to/wxWidgets-3.2.12/interface
```

The generator is documentation-only and does not require an installed wxWidgets
build. It preserves source declarations rather than attempting C++ semantic
resolution; declaration fragments and helper tokens intentionally remain visible.
Compare the resulting header hashes with the committed manifest when verifying
the same source version. Update `docs/coverage/status.json` when implementing
bindings; regeneration preserves those marks in the function lists and class
index. Do not edit generated checkboxes directly. A new upstream version requires
reviewing and migrating the exact declaration mappings before regeneration.
