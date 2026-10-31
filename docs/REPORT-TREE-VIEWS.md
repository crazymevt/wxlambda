# Report lists, native virtual text and tree interaction

Multi-selection subtree deletion preserves only surviving pre-delete selected
items, silently, including after polling. Deleting all selected items leaves an
empty selection; native MSW caret/parent fallback is not added to that set.
Single-selection tree deletion retains its separate documented ancestor fallback.

`list-control` is a native-backed WINDOW around `wxListCtrl` in REPORT and
single-selection mode. `:parent` is required. `:virtual-p` is fixed at construction
and defaults NIL. Factories validate `:class`; direct subclasses, slot/default
initargs, native-ready AFTER hooks and managed failure/root rollback follow the
normal CLOS contract. No application file, search-result or editor model is built
into this control.

```lisp
(defparameter report (wxlambda:make-list-control parent))
(wxlambda:insert-list-column report 0 "Name λ" :width 180)
(wxlambda:insert-list-column report 1 "Value" :width 120)
(wxlambda:append-item report '("First" "12")) ; new row index
(setf (wxlambda:list-item-text report 0 1) "13")
(wxlambda:list-item-text report 0 1)          ; independent string
(setf (wxlambda:selection report) 0)
(wxlambda:delete-item report 0)
```

The report starts with no columns/items. At most64 columns are supported; clear
items before inserting/deleting columns. Column captions are copied NUL-free UTF-8
strings of at most4096 bytes. `list-column-count` reads the native count;
`list-column-label` and `list-column-width` have generic SETF writers. Width writes
accept1..10000 native pixels, with native layout/readback policy; automatic width
scanning and arbitrary alignments/styles are not exposed.

Stored lists support at most100000 rows. `append-item` accepts a proper list of
strings matching the columns, with at most65536 UTF-8 bytes per cell and1MiB per
row. `list-item-text` reads a copied cell and SETF writes it. `item-count`,
`delete-item`, `clear-items` and `selection` specialize on the new class. Indices
are current zero-based positions, not stable row identities or item-data pointers.
Keep durable identities in application data. There is no native per-item Lisp
value/data ownership, label editing, checkboxes, multiselect, native sorting,
custom attributes/renderers or image list in this slice.

Programmatic selection is silent by default; `:notify t` permits the native
selected/deselected notification sequence, without promising a synthetic single
event or exact count. Selection reads return an index or NIL. NIL clears it.
Item/count mutations are silent. All list writes reject callbacks, including a
virtual text provider; structural changes require quiescence or an owner-bound
deferred UI action. Reads remain GUI-thread/lifetime checked. Native operations
and actual/propagated callbacks protect the list and ancestors against destruction,
close and nested GUI/modal/clipboard entry. Private native handler stacks remain
owned; managed stack access is not supported.

## Virtual REPORT lists

```lisp
(defparameter report
  (wxlambda:make-list-control parent :virtual-p t
    :item-text-function
    (lambda (control row column)
      (declare (ignore control))
      (format nil "Row ~D / column ~D" row column))))
(wxlambda:insert-list-column report 0 "Generated")
(setf (wxlambda:item-count report) 1000000)
(wxlambda:refresh-list-items report 0 100)
```

Only virtual lists support SETF `item-count`, from0 through10000000; positive
counts require columns. They do not allocate a Lisp row array or copy all row
strings into a native table. Native paint, layout, accessibility and explicit text
queries request cells as needed, so request counts/order are not stable. Providers
should read ready application data quickly. Update that data before publishing a
new count, then refresh changed ranges. Timers/deferred actions remain GUI-thread
mechanisms; worker results must reach application data through a separately safe
handoff. The provider is not a background worker or streaming scheduler.

Virtual lists require a registered public container parent chain: frame, panel,
scrolled-panel, drawing-canvas, web-view, notebook, aui-notebook or splitter-window.
Public Lisp subclasses retain their usual native identities and are supported.
Unregistered/native-private panels, arbitrary control ancestors and the built-in
modal prompt hierarchy reject before allocation. This bounds the inherited native
mutators that must preserve a pure text-provider query; stored report lists retain
the usual parent WINDOW contract.

The native virtual method is `wxListCtrl::OnGetItemText`. Its adapter invokes the
public CLOS `list-item-text` generic for the canonical receiver under a guarded
native callback. A subclass may specialize that generic instead of supplying
`:item-text-function`; the default virtual method calls the constructor function
with `(control row column)`, or returns an empty string if no function is supplied.
Nonvirtual defaults read the native stored text. Public virtual text queries pass
through the same native hook. Virtual SETF text/append/delete reject; change the
application provider data and native item count/refresh instead.

This is a native virtual-hook bridge, not a synthetic event. No event context or
raw wx pointer is passed. The function/control is strongly rooted by a numeric
token while native ownership is live. A scoped numeric request permits exactly
one synchronous UTF-8 reply, copied into native storage before the callback
returns; stale replies and replies from another nested callback reject. Returned
text is NUL-free and at most65536 UTF-8 bytes. Same-control recursive provider
queries reject. Ordinary Lisp errors retain their original condition; nonlocal
exits are converted to a contained error. The native hook returns empty text on
failure and Lisp re-signals at its safe surrounding boundary. No exception may
unwind through native paint/layout. An unexpected native bridge failure is also
contained and reported on the next list API boundary.

Provider calls suspend outer borrowed event contexts and hold source/ancestor
guards. Native list writes, inherited source/container relayout/visibility/enabled
and caption/name writes, destruction and nested GUI entry reject until return.
Native construction is unavailable during a provider. Other existing controls retain their callback contracts; providers should avoid
side effects. Parent destruction/accepted Destroy retires the provider before
composite teardown. Refused/throwing Destroy restores its guards. Session reaping
and shutdown release callback roots; late teardown requests never call Lisp.

## List notifications and context menus

Native-only notify kinds are `:list-item-selected`, `:list-item-deselected`,
`:list-item-activated`, `:list-item-right-click`, `:list-cache-hint` and
`:list-column-click`. `event-list-index` applies to the first four;
`event-list-column` to column clicks; `event-list-cache-range` returns a fresh
native `(from to)` pair for hints; `event-list-position` returns native client
coordinates for item right-click. Cache ranges are native hints, not provider
request/authorization guarantees. Activation/selection notifications are already
committed; veto metadata does not undo them. Item right-click is not a portable
keyboard context-menu API; the existing generic `:context-menu` input event remains
available for keyboard/native context requests.

Snapshots copy metadata; contexts are innermost-callback borrowed views. Payloads,
IDs and timestamps are read-only. Normal Skip/propagation controls remain. No
owned/synthetic/cloned list events or raw client/item data are exposed.

## Existing tree extensions

The existing `tree-control` and generation-checked canonical `tree-item` views now
support native `:tree-item-activated`, `:tree-item-menu`, `:tree-item-expanding`,
`:tree-item-expanded`, `:tree-item-collapsing` and `:tree-item-collapsed` events.
`event-tree-item` retains the checked item identity; no raw `wxTreeItemId` escapes.
`event-tree-position` returns a fresh native menu point; native keyboard/port
sentinels and coordinate policy are preserved. Item snapshots freeze identity,
not mutable label/decorations or lifetime.

EXPANDING/COLLAPSING can veto native changes. A consuming scoped handler must leave
Skip false when vetoing; skipped/unhandled events may ignore veto. EXPANDED and
COLLAPSED are observational. Same-tree writes and structural edits reject during
all these native callbacks, including ancestor propagation. Capture item identity
and defer edits or modal work. The existing wxQt tree construction gate remains.

```lisp
(setf (wxlambda:tree-item-bold-p item) t
      (wxlambda:tree-item-foreground item) #x28558a
      (wxlambda:tree-item-background item) #xf0f5fa)
(setf (wxlambda:tree-item-foreground item) nil) ; restore native default
(setf (wxlambda:tree-item-has-children-p item) t)
```

Bold is a generalized boolean. Colours are copied packed0xRRGGBB integers or NIL
when the native colour is invalid. SETF NIL clears an explicit colour; some ports return the effective default RGB afterward. `tree-item-has-children-p` maps the native
potential-children/expander hint, not the actual child-list length; clearing it
does not delete children. This supports a lazy-loading pattern: mark a possible
branch, veto its first expansion, defer loading/insertion, then request expansion
after data is ready. It does not add a filesystem or lazy model abstraction.
Owned/synthetic/cloned tree event families remain unsupported.

Native prerequisites: wxUSE_LISTCTRL and existing wxUSE_TREECTRL/core. Native GUI
calls and provider roots are main-thread/session owned; physical input, hosted
GTK/MSW/Qt behavior and accessibility fidelity require separate checks. This is a
bounded wxWidgets3.2.12 list/tree adapter, not complete ListCtrl/DataView parity.

## Example and checks

Run `sbcl --load ~/quicklisp/setup.lisp --load examples/report-views.lisp --eval '(wxlambda-report-views:start)'`. The `exercise` entry point checks the million-row provider and copied values without manual input. `tests/report-views.lisp` additionally verifies CLOS subclass initialization/rollback, provider errors/nonlocal containment, suspended outer contexts, tree veto and root cleanup. The native harness checks native list selection and tree expansion, nested supported containers, read-only provider ancestor operations and callback retirement. These derived event fixtures do not claim physical gesture coverage.

ABI68 adds `:tree-begin-drag` source item/client point snapshots. Built-in dragging
remains vetoed; see [Guarded tree dragging](TREE-DRAG.md) for synchronous
`drag-tree-item` in its BEGIN callback and native lifetime boundaries.

## Multiple tree selection (ABI70)

`make-tree-control :multiple-p t` uses wxTR_MULTIPLE. `tree-selections` returns
all selected items; its SETF silently replaces the set after validating every
item and rejects duplicates/foreign/stale items. Empty selection is supported.
`selection` returns the first selected item; its SETF replaces the set with one
item or NIL. Default single-select trees retain their existing behavior.

Native range/toggle mouse selection works through the platform tree control.
Appending items preserves selection without scanning the selected set. Expanding
or collapsing does not force a selected item; deleting entries preserves surviving
selections. Drag APIs still return one source/target; the application snapshots
`tree-selections` in the contextual BEGIN callback to implement batch operations.
See `examples/multi-tree.lisp` and `tests/multi-tree.lisp`.
