# Native styled text control

ABI57 binds `wxStyledTextCtrl` as `styled-text-control`, a WINDOW subtype with
native-backed CLOS construction. `make-styled-text-control` is a thin generic
factory with `:class` and forwarded subclass initargs. Required `:parent` is a
live WINDOW; `:value` defaults to the empty string. Subclasses support slots,
`:default-initargs`, native properties in AFTER hooks and managed failure rollback.
The `stc` component and `wxUSE_STC` are build prerequisites.

The binding exposes native editing/rendering facilities. It does not implement
document buffers, parsing, highlighting decisions, paredit, refactoring, key
sequences, language services or file IO. Those belong in the application.

## Positions, encoding and changes

The native code page is fixed to `wxSTC_CP_UTF8`. Every document position,
selection endpoint and changed-range length is a **UTF-8 byte offset**, not a
Lisp string index, UTF-16 offset, glyph number or screen column. Ranges are
half-open `[start,end)`. Line indices are zero-based. Native columns include tab
expansion policy and are not portable character indices. Point conversions use
native client coordinates, not global screen coordinates or automatic DIP units.

`stc-text` reads a fresh string; generic SETF replaces the document through
native target-range editing. `stc-text-range` retrieves a checked range and
`replace-stc-range` replaces one. The bridge validates endpoints at UTF-8
character boundaries before native calls. Explicit raw lengths preserve embedded
NUL and supplementary characters. Native inputs must be valid UTF-8; a native
string/pointer never escapes to Lisp. Programmatic document edits are bounded to
64MiB; native user editing is not capped by that programmatic bound. Native
position arguments use signed32-bit values. Borrowed getter storage is copied
before another native call. GUI-thread/session/lifetime checks apply to reads too.

Unlike the older text-control silent setters, STC edits preserve actual Scintilla
modification notifications. `:stc-modified` snapshots copy `event-text`, byte
position/length, modification bit mask, lines added, native line and fold levels.
Before-insert/delete flags describe impending changes; insert/delete flags
describe completed changes. Notifications can also describe style/indicator/
marker changes without inserted text. Do not treat every notification as one
completed edit. Undo/redo and native input use the same native notification path.
`stc-modification-mask` maps native `SetModEventMask`; applications control the mask.

`event-stc-position`, `event-stc-length`, `event-stc-lines-added`,
`event-stc-modification-type`, `event-stc-line`, `event-stc-fold-level-now` and
`event-stc-fold-level-previous` expose the corresponding native fields. Readers
reject event kinds where the pinned wxWidgets API does not define the field.
Snapshots remain usable after source destruction/session cleanup; contexts are
borrowed only for the current innermost callback. Changed text is an independent
string, including embedded NUL. Snapshot identity does not freeze the live document.

## Selection, undo and edit policy

`stc-current-position` and `stc-anchor` have generic SETF writers.
`set-stc-selection` takes anchor/caret; it uses the native selection operation.
`stc-selection-count`, indexed `stc-selection-caret`/`stc-selection-anchor`,
`stc-main-selection`, `add-stc-selection`, `drop-stc-selection` and
`clear-stc-selections` expose native multiple-selection state. At least one
selection is retained. `stc-multiple-selection-p` and
`stc-additional-selection-typing-p` control native interaction policy.

`stc-selection-mode` uses native values0 stream,1 rectangle,2 lines,3 thin
rectangle. Rectangular anchor/caret and their virtual-space properties map native
methods. `stc-virtual-space-options` accepts the native bit mask0..7. Virtual-space
counts are not additional document bytes. These APIs do not implement application
multi-cursor editing policy or promise identical gestures on every backend.

`stc-undo`, `stc-redo`, capability readers, `begin-stc-undo-action`,
`end-stc-undo-action`, `empty-stc-undo-buffer`, `set-stc-save-point` and
`stc-modified-p` use Scintilla history/save points. Pair grouped undo with
UNWIND-PROTECT; unmatched END and nesting beyond64 reject. Emptying history during
an active group rejects. Save points are native history state, not successful
filesystem saves. `stc-read-only-p`/SETF maps native read-only policy; bridge text
edits and undo/redo reject while read-only. There is no implicit temporary bypass.

## Application-controlled styling and decorations

Construction selects `wxSTC_LEX_CONTAINER`, not a built-in language lexer.
Applications supply style decisions using `start-stc-styling` and
`set-stc-styling`; lengths count UTF-8 bytes. `stc-end-styled` reads the native
styling frontier for incremental work. `set-stc-style-bytes` copies a simple
unsigned-byte8 vector and calls native SetStyleBytes, one style byte per text
byte, up to64MiB and within the current styling range. `stc-style-at` reads the native
style. `clear-stc-document-style` clears document styling; `clear-stc-styles`
copies the native default style to style definitions (`StyleClearAll`). They are
different wxWidgets operations. Styles0..255 support foreground/background
packed RGB (`#xRRGGBB`), font face, size1..256 points, bold, italic and underline.
Exact fonts, glyph fallback and rendering remain native policy.

The exception to ordinary callback mutation rejection is deliberately narrow:
the current innermost actual **same-control `:stc-style-needed` callback** may
call StartStyling/SetStyling/SetStyleBytes synchronously. This is the native container-styling
protocol. It permits no document edits, cross-control styling, property writes
or reentrant styling from nested modification/input callbacks. Receiver/ancestor
destruction and nested GUI/modal/clipboard entry remain guarded. Other styling
work can use an owner-bound deferred action and recheck current document state.

Indicators0..35 have style/foreground/under properties, a current indicator and
value, and fill/clear range operations. Indicator styles use native values0..21.
Margins0..4 expose type0..6, pixel width, signed native mask and sensitivity.
For line numbers, use native `wxSTC_MARGIN_NUMBER` (1) and a nonzero margin width.
Markers0..31 support native symbol definitions0..31, add/delete/delete-all and
per-line marker masks. Marker-add results are native marker handles, not windows
or adopted resources. Marker31 masks are read as unsigned32-bit values.

`stc-fold-level` combines native level/flags within0..#x3fff (base#x400,
white#x1000, header#x2000). Expanded/parent/visibility queries, toggle-fold,
show/hide-lines and ensure-visible map native methods. The first line cannot be
hidden through the bridge. The application supplies fold levels; no parser or
automatic folding decisions are added. Wrapped display lines are outside this
initial scope: native wrap mode is fixed to NONE.

## Coordinates, scrolling and assistance

Line/position/column conversions, document/visible-line conversions,
`stc-point-from-position` and `stc-position-from-point` expose checked native
operations. A point not close to text returns NIL. `stc-first-visible-line`,
`stc-x-offset`, `scroll-stc-lines` and `ensure-stc-caret-visible` expose native
scrolling. Explicit scroll deltas are limited to ±1000000. Other nonnegative
scalar counts/widths/times are bounded to10000000; signed native masks retain
signed32-bit semantics. Document byte ranges have their separate64MiB limit.

`show-stc-autocomplete` takes a native byte-prefix length and native item-list
string (default separator space). Prefix start must be a UTF-8 boundary.
Separator1..127, active/cancel/complete operations map native autocomplete.
`show-stc-call-tip` takes a byte position and native text; active/cancel queries
map native call tips. Strings reject NUL and are bounded to1MiB. Completion lists,
language intelligence and signatures come from the application.

## Events and lifetime rules

Use ordinary `bind-event`:

| Kind | Native meaning / fields |
| --- | --- |
| `:stc-modified` | Changed text/ranges, modification mask, lines/fold metadata |
| `:stc-update-ui` | `event-stc-update-mask`: content/selection/scroll update flags |
| `:stc-style-needed` | Requested end byte position; manual styling protocol |
| `:stc-margin-click`, `:stc-margin-right-click` | Byte position, margin and native modifier mask |
| `:stc-dwell-start`, `:stc-dwell-end` | Byte position and native client x/y; dwell time property |
| `:stc-save-point-reached`, `:stc-save-point-left` | Native save-point transitions |
| `:stc-autocomplete-selection`, `:stc-autocomplete-completed` | Native completion text and byte position |
| `:stc-call-tip-click` | Native position/arrow indication, not an application command |
| `:stc-need-shown` | Byte position/length requiring visibility |
| `:stc-read-only-attempt` | Native refused edit notification |
| `:stc-char-added` | Native `event-stc-key`; not a portable IME composition event |

Observers automatically Skip. `:with-context t` receives snapshot/context and
controls Skip/propagation explicitly. These are native command events, with
read-only payload metadata; no synthetic/owned/cloned STC events or native
pointer/clientData adoption. Typed reads expose only appropriate native fields.
All mutations except the narrow styling protocol reject event callback use.
Use `defer-ui-action` for structural or deferred changes. Private native handler
stacks cannot be replaced with managed Push/Pop/Remove operations.

Native controls retain normal text input/IME handling when events Skip. Consuming
keys/characters changes that behavior; no portable IME composition API is claimed.
Automatic tests cover real UTF-8/NUL edits/notifications, undo, selections,
styling/folding/assistance, checked lifetimes and callback rules. Physical typing,
IME/dead keys, arbitrary fonts/DPI and hosted Windows/Linux/Qt remain unverified.
See the [survey/remaining batches](KRYPTONEDIT-COVERAGE.md),
[input routing](INPUT-EVENTS.md), [AUI tabs](AUI-NOTEBOOK.md),
[example](../examples/styled-text.lisp) and [Cadre docset](DOCSET.md).
