# General-purpose wxWidgets capabilities for KryptonEdit

Survey baseline: clean commit `371edbe`, ABI56. Grid/WebView were already committed;
there were no in-progress source changes to discard. This roadmap preserves that
work and supersedes the old “no next required batch” handoff for this explicit
application request. It does not supersede the completed local MVP checkpoint.
wxLambda remains general-purpose: only native wxWidgets controls/methods/properties/
events are binding targets. Parsing, highlighting decisions, paredit, refactoring,
language services, document/workspace policy and persistence belong to KryptonEdit.
No terminal emulator is planned: wxWidgets has no standard full terminal-emulator
control. A terminal requires application-level integration.

The authoritative scope is the pinned3.2.12 interface inventory and selected
native implementations, not declaration totals interpreted as parity percentages.
Local macOS SBCL/CCL results do not certify Windows/Linux/Qt or physical IME.

## Prioritized delivery batches

| Order | General native capability | Baseline / required dependency |
| --- | --- | --- |
| 1 | wxStyledTextCtrl UTF-8 text/ranges, modification payloads, selection/undo/style/fold/assist APIs | Previously unbound; link `stc`, require `wxUSE_STC`. Container/manual styling must work without a built-in lexer. Native positions are UTF-8 bytes, not Lisp character indices. |
| 2 | Raw input/command routing | Command/notify/close/idle only at baseline; add appropriate raw event classes and immutable fields, preserve native Skip and IME defaults. |
| 3 | wxAuiNotebook and dynamic splitters | Fixed notebook/two-pane splitter only; link `aui`, require `wxUSE_AUI`; page removal must differ from deletion and reparenting/drag policy must be explicit. |
| 4 | Tree/list/data-view/grid depth | Bounded existing tree/grid; no listctrl/dataview. Native model callback ownership and scalable data are separate focused work. |
| 5 | Web navigation/HTML/images/drawing | Existing audited macOS interactive WebView and private retained renderer; broader native resources and navigation events remain gaps. |
| 6 | Worker-to-GUI delivery | Main-thread deferred actions/timers already exist; they are not a worker API. Bind a checked wxThreadEvent/wxQueueEvent delivery path. |

Baseline header mappings: STC0, AUI notebook/manager0, listctrl/dataview0, HTML0,
thread0; grid15 partial declarations, WebView12 partial, tree20 partial, splitter18
partial. Existing image2/bitmap4/DC6 partial adapters are private retained-rendering
support, not public image/DC resource coverage. These are mapped declarations,
not function totals or completion percentages; see the generated inventory for
current statuses after each batch.

The installed local3.2.12 configuration has STC/AUI/HTML/THREADS enabled and their
libraries available. New CMake linkage is required where components are selected.
ABI57 implements the first three rows in bounded scope; ABI58 adds the selected
worker delivery row. See the current delivery record below. Other rows remain
pending until separately implemented.

## Priority input events for KryptonEdit

Survey target: pinned wxWidgets3.2.12, current bridge at371edbe. This is an
implementation recommendation, not a claim that these bindings already exist.
The pinned interface sources are
[event.h](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/event.h),
[kbdstate.h](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/kbdstate.h),
[mousestate.h](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/mousestate.h)
and [STC](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/stc/stc.h).

### Recommended useful initial surface

Reuse BIND-EVENT, immutable EVENT snapshots and scoped EVENT-CONTEXT, instead of
adding separate per-widget key/mouse callback systems. Bind on WINDOW receivers;
do not make parent bindings imply that every raw child input event propagates.

| Native kinds | Suggested Lisp kinds | Read-only copied payload |
| --- | --- | --- |
| KEY_DOWN, KEY_UP, CHAR, CHAR_HOOK | :key-down, :key-up, :char, :char-hook | native key code, Unicode code point or NIL for WXK_NONE, raw key code/flags, modifiers, auto-repeat, client x/y |
| LEFT/MIDDLE/RIGHT_DOWN/UP/DCLICK; AUX1/AUX2 variants | :left-down/:left-up/:left-double-click etc. | client x/y, triggering button, current buttons-down, modifiers, native click count |
| MOTION, ENTER_WINDOW, LEAVE_WINDOW | :mouse-motion, :mouse-enter, :mouse-leave | client x/y, buttons-down, modifiers; dragging derived from native Dragging() or copied explicitly |
| MOUSEWHEEL | :mouse-wheel | mouse payload plus signed rotation, delta, axis, lines/columns per action, page-scroll, inverted |
| SET_FOCUS, KILL_FOCUS | :focus-gained, :focus-lost | source bridge identity; optional related registered-window ID only, never raw GetWindow pointer |
| CONTEXT_MENU | :context-menu | screen coordinates, or explicit NIL for wxDefaultPosition keyboard-origin sentinel |
| SIZE | :size | copied whole-window width/height, distinct from client extent and DIP conversion |

Add ordinary generic event readers, e.g. EVENT-KEY-CODE, EVENT-UNICODE-KEY,
EVENT-RAW-KEY-CODE/FLAGS, EVENT-MODIFIERS, EVENT-AUTO-REPEAT-P,
EVENT-POSITION, EVENT-MOUSE-BUTTON, EVENT-MOUSE-BUTTONS, EVENT-WHEEL-ROTATION,
EVENT-WHEEL-DELTA/AXIS, EVENT-WHEEL-LINES/COLUMNS/PAGE-SCROLL-P/INVERTED-P,
EVENT-RELATED-WINDOW-ID and EVENT-SIZE. Exact names can be consolidated before
implementation. Fresh tuples/bitfield decoding preserve immutable snapshots.
Kind-check readers rather than treating absent data as misleading valid zero.
Use numeric native key codes plus a documented bounded keyword mapping for
common keys; avoid exposing hundreds of wx constants merely to handle shortcuts.
Expose explicit modifier distinctions: native GetModifiers plus CmdDown and
RawControlDown semantics differ on macOS. Raw flags are platform-specific.

### Default input and propagation are correctness requirements

Current Binding::on_event automatically Skip()s observer callbacks. Preserve this
for raw input: observing a key must not disable text entry, accelerators or native
selection. Scoped contextual bindings already begin consumed (Skip(false)); this
is useful for intentional key interception but must be conspicuous in docs.
Consuming KEY_DOWN prevents corresponding CHAR and can suppress accelerators;
KEY_UP need not match each repeated KEY_DOWN. Consume only recognized shortcuts,
Skip everything else. Prefer translated CHAR for character information and
KEY_DOWN for physical/logical shortcut handling; neither is a full text editor.

CHAR_HOOK uniquely propagates upward before normal key events; consuming it
normally suppresses KEY_DOWN/CHAR. A tightly scoped context-only operation for
DoAllowNextEvent()/IsNextEventAllowed may be warranted: it can stop ancestor
hooks while allowing normal input. Do not overload ordinary Skip with this
different native behavior. Ordinary key/mouse/focus/size events are wxEvent
subclasses, not command events, and do not generally bubble like button commands.
CONTEXT_MENU is wxCommandEvent and has normal command propagation.

Focus and SIZE handlers should almost always Skip: otherwise native focus
behavior or sizer layout can break. Never synchronously refocus from KILL_FOCUS;
existing deferred FOCUS-WINDOW operation is appropriate. Preserve existing
callback mutation protections. Context menus should use CONTEXT_MENU, not infer
platform timing from RIGHT_DOWN/UP: Windows and GTK generate them at different
points, and consuming a right-button event can suppress the native context event.

### Lifetime and bridge integration

Current native_event_family only handles command/notify/close/idle. Add a truthful
base family (:event or named raw families); do not claim raw key/mouse/focus/size
events are command events. Update all Lisp family decoding and context readers.
Append ABI fields or a bounded copied payload record; update native struct, FFI,
snapshot construction, immutable Lisp representation, context getters and ABI
version together. Raw accessors must not call command-only GetString/GetInt.

InputCallbackOperation currently protects selected control-origin events using
input_owners and actual/child origins. General raw events must protect the actual
source and its ancestors even when the binding is on a propagated parent or a
pushed handler. Resolve registered source safely; composite native children must
map to their managed owner without publishing borrowed child wrappers. Apply
same pending/scheduled/early-teardown suppression as existing input controls.
Keep innermost context tokens, original snapshot identity, callback error
containment and user-data rooting. Destruction, modal/clipboard entry, focus and
nested native pumps remain deferred during raw input. Native size/focus may fire
synchronously during creation/layout/destruction: suppress before publication
and during base teardown; rollback must remove callback roots.

Initially keep raw native events nonconstructible/noncloneable, with immutable
key/mouse/size/focus payloads. Existing context Skip/propagation operations remain
available; no blanket payload SETF gate that accidentally blocks those operations.
No raw focus/window pointers or OS event injection. Capture/release mouse,
capture-lost and gesture/touch APIs can be a separate coherent followup if a
custom-dragging application requires them; don't imply drag tracking continues
outside a window without capture.

### IME boundary

The pinned generic event surface does not provide a portable composition start/
update/end or marked-text range payload. CHAR Unicode data is not an IME
composition API and must not be advertised as one. Dead keys, non-Latin input,
emoji/supplementary characters, candidate selection and replacement ranges need
native text-control/STC handling. STC has Get/SetIMEInteraction (windowed/inline),
but that is a presentation policy, not Lisp composition callbacks. Let STC own
native composition; use editor text/modification notifications for committed
document state. Automated synthetic key events are insufficient evidence of IME
support. Physical macOS IME/dead-key/emoji tests and hosted Windows/GTK IME tests
remain explicit unverified targets until performed.

### Acceptance

Native fixtures should dispatch actual wxKeyEvent/wxMouseEvent/wxFocusEvent/
wxContextMenuEvent/wxSizeEvent classes through generic bindings to verify copied
payloads, no raw pointer exposure, kind errors and context lifetime. Check observer
Skip, intentional consume, hook allow-next if exposed, parent command/hook vs
nonpropagating raw events, and payload immutability. Test source/ancestor deletion,
propagated/pushed bindings, nested native entry, creation/teardown suppression and
error unwinding. Add an editor input inspector example showing modifier/key/char,
mouse/wheel/focus/size events without consuming native editing. Physical typing,
selection, shortcut, wheel, context-menu and IME acceptance stays separate from
fixture coverage. Both-Lisp suites, native zero-assertions, examples, factual
native coverage and zero-problem docset remain delivery gates.


## Baseline workspace, navigation, tables, background work and images

Survey baseline: clean ABI56 checkpoint `371edbe`, pinned wxWidgets3.2.12 interface
inventory and reviewed `src/aui/auibook.cpp` / `src/generic/splitter.cpp`. These are
general-purpose binding capabilities; document buffers, projects, search indexes,
language servers, persistence and editor policy belong in the application.

| Area | Existing binding | Missing native-facing capability / useful next scope |
| --- | --- | --- |
| Workspace tabs | `notebook`: canonical owned PANEL pages, captions, silent/notifying selection and vetoable native page events; safe deferred actions permit AddPage after callbacks return. | No insertion, removal/deletion, reordering, close buttons or alternate book. Add a distinct `aui-notebook` WINDOW with dynamic owned pages and actual close/change events; do not confuse CLOS ancestry with native wxNotebook. |
| Split workspace | `splitter-window`: two fixed PANEL panes, immutable orientation, sash position/minimum/gravity and guarded sash notifications. | No single-pane initialization, unsplit/re-split or replacement. Bounded `Initialize`, `Unsplit` and `ReplaceWindow` adapters can retain hidden removed panes without deleting or reparenting them. No docking manager is needed for a two-pane workspace. |
| Project tree | Single-selection `tree-control`, canonical generation-checked item views, copied labels, append/delete/traversal/expand and native selection snapshots. | No icons/image lists, lazy expansion notifications, activation/context menu, drag/drop, label editing, hidden-root, multiselect or virtual model. Directory traversal can already use Lisp IO/model code; large/lazy trees need expansion and activation events plus bounded item styling/images. wxQt stays unavailable until raw asynchronous item events are generation safe. |
| Results list/table | Dynamic single-selection unsorted `list-box` with append/rename/delete/clear, selection and double-click. `grid` has a fixed native-owned string table, copied cell/label text, editing policy and committed-value readback. | No multi-column report/virtual list (`wxListCtrl`/`wxListView`), data view/tree list, grid cell/selection events, dynamic grid shape or custom model ownership. A nonvirtual report-list with copied columns/rows and selection/activation would be a smaller reusable first results-table slice than Lisp virtual-model callbacks. |
| Background activity | Owner-bound deferred UI actions/timers deliver at safe main-thread idle; activity indicator supplies feedback. ABI58 `queue-thread-event` copies bounded strings from workers to existing GUI-registered receivers; callbacks run on the GUI thread. | No process IO/subprocess abstraction, filesystem watcher or worker scheduler. Other native operations, including `defer-ui-action`, timers and `wake-up-idle`, retain GUI-thread rules. No arbitrary Lisp object/native pointer transfer; see [thread events](THREAD-EVENTS.md). |
| Images/icons | Retained canvas supports copied opaque RGB buffers (each bitmap1..256, total2MiB) and private PNG export; native DC/font/bitmap/image resources remain hidden. | No image file decode, alpha, scaling, standalone image/bitmap resource, static-bitmap widget, image lists, toolbar/tree/tab icons or DPI bitmap bundles. Introduce copied RGBA image values and a small native display/icon adapter only when selected; avoid raw resource adoption/global handler mutation. Decode failures, byte/pixel limits, alpha/scale/DPI and disposal need explicit contracts. |

Dependencies and platform boundaries: current CMake links core/base/adv/webview;
`wxAuiNotebook` additionally requires the `aui` component and `wxUSE_AUI=1`
(`wxUSE_AUI=1` is present locally but linking is not selected). List/report and data
view controls require `wxUSE_LISTCTRL` / `wxUSE_DATAVIEWCTRL`; both are enabled in
the installed build but unbound. Images already have IMAGE/LIBPNG prerequisites;
new formats/handlers and bitmap bundles need selected feature checks, not a claim
of blanket image parity. File watchers require `wxUSE_FSWATCHER` and copied
platform-dependent event semantics. Local feature availability does not establish
hosted GTK/MSW correctness. Existing WebView remains specifically gated to verified
macOS3.2.12 WKWebView; it does not supply cross-thread UI or workspace ownership.

### Original dynamic-tab proposal (superseded by ABI57 scope)

1. Add native-backed `aui-notebook` as a separate WINDOW class with validated
   `:parent`/`:class` construction, subclass/default initargs and managed rollback.
   Reuse existing notebook generics where their semantics truly match: page count,
   page lookup, caption and `selection`. Accept owned PANEL pages initially so any
   editor/control can be composed inside a page without an editor abstraction.
2. Add guarded append/insert/delete plus page-to-current-index lookup. Require
   native parent==book, no sizer/membership and fully completed page initialization.
   Root canonical wrappers before native publication, retain membership by window
   identity across reorder, and invalidate on actual native destruction. Resolve
   a retained page identity at deferred delivery; indices are not durable IDs.
   Defer remove-without-delete/reparent transfer until ownership semantics are
   deliberately chosen; native RemovePage leaves the page alive and parented.
3. Choose explicit initial styles: top tabs, scrolling, same-book tab move and
   active-tab close. Disable external transfer and internal tab splitting first;
   default native styles enable more behavior than this contract. Do not expose
   raw art providers, tab controls, AUI manager/perspective resources or arbitrary
   flags. Programmatic page deletion does not inherently run the user-close veto
   path; its API must say so or deliberately implement a separate close request.
4. Bind actual `wxAuiNotebookEvent` page-changing/changed and page-close request.
   Changing/close veto is scoped to its native event; a modal confirmation must
   first veto and schedule an owner-bound action. Native close later deletes the
   page. Guard the originating book, affected pages and ancestors throughout
   native operations and actual/propagated callbacks, including private tab-child
   origins. Reject callback structural mutation, same-book selection recursion,
   destructive close/reentry and owned/synthetic/cloned AUI events. Capture a
   stable target before deferral; native PAGE_CLOSED reports an old index after
   deletion, so it cannot be treated as a live current-page lookup. Omit that
   public event initially unless its removed-page identity contract is explicit.
5. Audit internal manager/tab handlers before allowing managed handler-stack edits;
   default to rejecting them. Test native UI close/veto, reorder, selection,
   last-page deletion, callback failure, constructor failure, stale wrappers,
   ancestor teardown and zero native assertions. Add a general dynamic-workspace
   example with arbitrary panel content; include SBCL/CCL, coverage and docset.

For the matching splitter extension, retain the current immutable orientation and
positive minimum size. Add a safe single-pane initializer, explicit unsplit and
replacement with canonical pane identity. Removed/replaced panes remain hidden,
alive and native children of the splitter until explicitly reused/destroyed; they
are no longer attached members. Validate distinct initialized same-parent panels,
prepublish new roots, and protect old/new panes plus ancestors across synchronous
sizing/focus callbacks. Native ReplaceWindow does not destroy **or automatically
hide** the old pane; the bridge must specify and perform hiding. Unsplit hides its
removed pane by default and need not emit the same notification path as a native
sash-triggered unsplit. Treat interactive unsplitting and dynamic orientation as
later scope. This should remain a reusable window operation, not an editor layout
or document lifecycle API.

Pinned references: [AUI notebook interface](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/aui/auibook.h),
[AUI implementation](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/aui/auibook.cpp),
[splitter interface](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/splitter.h),
[splitter implementation](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/generic/splitter.cpp),
[list controls](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/listctrl.h),
[data views](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/dataview.h),
[images](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/image.h),
[filesystem watcher](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/fswatcher.h).

## Current delivery: ABI57–ABI61 selected native bindings

Implemented: native-backed styled text with UTF-8 byte ranges, modification
metadata, multiple/rectangular selection, undo/save points, application styling,
indicators/margins/markers/folding, coordinate conversion, scrolling, completion
and call tips; copied STC events. Native key/character/mouse/wheel/focus/context/
size events expose scoped Skip/propagation and CHAR_HOOK AllowNextKeyEvent.
AUI tabs support insert/remove/delete/move and programmatic splitting; splitters
support initialize/unsplit/replacement/resplit and mutable orientation. See
[styled text](STYLED-TEXT.md), [input](INPUT-EVENTS.md),
[AUI](AUI-NOTEBOOK.md) and [splitters](SPLITTER.md) for exact contracts.

Unlike the baseline proposal, removal without deletion and PAGE_CLOSED are bound
with explicit stable identities and cache cleanup. Programmatic AUI splitting is
supported; physical tab splitting and external transfer remain disabled. Physical
tab rearrangement is restricted to one private tab group because pinned native
code mixes local/global indices across groups. Private tab-ID allocation is
bounded to501 groups per book lifetime. The current guides supersede baseline
recommendations above. No exhaustive native parity is implied.

Final91-scenario GUI suites plus preflight pass on both SBCL and CCL, alongside
native fixtures, isolated package checks and the661-entry docset. Evidence is
local macOS wxWidgets3.2.12; physical keyboard/IME/drag/accessibility and other
ports remain unverified. These are binding and automated acceptance results,
not a claim of every native argument combination or physical input fidelity.

ABI59 adds native report LIST-CONTROL with stored rows or a copied synchronous
OnGetItemText provider, selection/activation/context/cache notifications, and tree
activation/context/expansion events plus bold/color/expander decorations. Virtual
providers run as guarded pure queries with explicit supported container ancestry;
see [report/tree views](REPORT-TREE-VIEWS.md). wxDataViewCtrl and custom native
model ownership remain unbound; virtual wxListCtrl supplies the selected scalable
results-table path.

ABI60 adds session-owned wxImage/wxBitmap, native static bitmap display, copied
RGB/alpha/pixel access, PNG loading/saving, scaling and native display/coordinate
queries. Bitmap backing pixels, logical units, DIP and scale remain distinct.
The retained wxDC renderer accepts copied RGBA images. No image-grid/flame-graph
application model is provided. See [images/displays](IMAGES-DISPLAYS.md) and
[drawing](DRAWING-CANVAS.md). PNG-only file loading and resource/scene bounds are
explicit supported scope; physical color fidelity, hotplug and other ports are
unverified.

ABI58 adds `queue-thread-event` and native `:thread` delivery through existing
EVENT-HANDLER receivers. Registration/binding stays on the GUI thread; workers
copy only bounded UTF-8 payloads through a separately synchronized native endpoint.
Receiver retirement, pending-event deletion and shutdown serialize against posts;
callbacks stay on the GUI thread. Focused both-Lisp worker tests and native race
fixtures pass. See [thread events](THREAD-EVENTS.md) for exact limits and evidence
in the implementation handoff; this is not a worker scheduler or editor job API.

ABI61 completes the selected GRID depth: native cell/selection/range/editor/click
events with copied proposed/old/current values, scoped validation veto, per-cell
read-only/color attributes and dynamic native string-table dimensions (axes0..256
after deletion, at most16384 cells). Save, Hide, Disable and current-editor Reset
retain native semantics; Hide can cause a deferred commit and Reset is not a
transaction rollback. See [grid editing](GRID-EDITING.md) and
[the pinned editor audit](GRID-EDITOR-AUDIT.md). Custom models/editors remain unbound.

**The primary selected binding goal is complete within this documented scope.**
KryptonEdit application integration is the next step; no additional broad binding
batch is required. Add further native capabilities only for a concrete application
need. The survey-only candidates and verification limits below remain explicit.

## Docking and lightweight HTML survey

`wxAuiManager` remains unbound. Pinned3.2.12 provides AddPane/InsertPane/DetachPane,
wxAuiPaneInfo descriptions, Update, pane events and SavePerspective/LoadPerspective.
A reusable binding needs managed manager/window lifetimes, explicit UnInit before
managed-window destruction, canonical pane ownership and floating-frame teardown,
scoped pane-close veto and guarded layout updates. DetachPane is distinct from
window destruction. Perspectives are opaque native strings tied to pane names;
KryptonEdit owns naming, storage and restore policy. It requires AUI, already linked
for notebook support; macOS floating panes and hosted-port layout behavior need
separate evidence. It is surveyed, not implemented by the notebook slice.

`wxHtmlWindow` remains unbound. SetPage/AppendToPage, LoadPage/LoadFile, history,
opened-page/title and link events provide lightweight rendered documentation. It
uses wxHTML and wxFileSystem resource loading, without WebView JavaScript or modern
browser rendering. A future binding must specify resource URL policy, copied link
payloads, OnOpeningURL/OnLinkClicked callback scope and filesystem ownership.
The installed HTML component is available but not currently linked by this bridge.

Interactive WebView is already implemented within the audited macOS3.2.12
WKWebView gate. LOADED/error/script-message and copied asynchronous
script results do not supply complete navigation policy. NAVIGATING cancellation,
NAVIGATED/title/new-window events, redirect/navigation cancellation and unsupported
platforms need separate source/lifetime audits. Basic CanGoBack/Forward and
GoBack/Forward are already bound; richer history metadata/policy remain future
scope; native LOADED is not application readiness. Existing
platform constraints remain in [WebView](WEB-VIEW.md).

Pinned references: [AUI manager](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/aui/framemanager.h),
[HTML window](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/html/htmlwin.h).

## Audited grid editing and owned table changes

ABI61 adds [native grid editing](GRID-EDITING.md): selected native notify events,
scoped validation, cursor/block selection requests, per-cell read-only/colour
attributes and bounded append/insert/delete of owned string-table axes. Native
Save/Hide/Disable/Reset semantics remain distinct; no invented cancellation or
transaction guarantee, custom model/editor ownership or full native parity is
claimed. Empty axes are supported after dimension edits. This closes the selected
bounded grid binding dependency; application integration and unverified physical
interaction/IME/accessibility/other-port behavior remain separate.
