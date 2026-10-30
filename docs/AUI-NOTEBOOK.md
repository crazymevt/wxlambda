# Dynamic AUI notebooks and splitter panes

`aui-notebook` is a native-backed WINDOW class for general dynamic window
workspaces. It owns PANEL pages; put any supported controls or layouts inside
those panels. It supplies no editor buffer, document, file, project or persistence
abstraction. The existing `notebook` retains its separate native class and scope.

```lisp
(defparameter book (wxlambda:make-aui-notebook parent))
(defparameter page (wxlambda:make-panel book))
(wxlambda:add-notebook-page book page :label "Tools λ" :select t)
(wxlambda:insert-notebook-page book another-page 0 :label "Overview")
(wxlambda:notebook-page-count book)
(wxlambda:notebook-page book 0)          ; canonical EQ page wrapper
(wxlambda:notebook-page-index book page) ; current index or NIL
(setf (wxlambda:notebook-page-label book 0) "Overview λ")
(setf (wxlambda:selection book) 0)        ; silent native selection
(setf (wxlambda:selection book :notify t) 1) ; native vetoable navigation
(wxlambda:move-notebook-page book 1 0)    ; silent, selected identity preserved
(wxlambda:split-notebook-page book 1 :right) ; native tab group, same page owner
(wxlambda:remove-notebook-page book 0)   ; returns hidden, live PANEL
(wxlambda:delete-notebook-page book 0)   ; destroys selected indexed page; returns T
```

Construction requires a live `:parent` WINDOW. The thin factory validates `:class`
and forwards subclass initargs. Direct subclasses/default initargs/native-ready
AFTER hooks and managed initializer failure/root rollback follow the CLOS API.
The native `aui` library and `wxUSE_AUI=1` are build prerequisites.

Tabs use a fixed top-tab style with scrolling, an active-tab close button and
mouse reordering while the notebook has one tab group. Programmatic native
splitting and movement are described below. External notebook transfer/reparenting,
mouse drag splitting, middle-button close, arbitrary style flags, art providers, page icons,
raw tab/manager resources and docking/perspective APIs are not exposed. Native
dragging changes current indices; retain canonical page wrappers and ask
`notebook-page-index` when acting later. No operation turns Lisp subclassing into
native C++ virtual callbacks.

`split-notebook-page` calls native `wxAuiNotebook::Split` with a current global page
index and one of `:left`, `:right`, `:top` or `:bottom`; it returns BOOK. At least
two pages are required. Native private tab groups are created and arranged without
reparenting page windows: each page remains a direct child of the same book, with
the same wrapper, bridge ID and global catalogue index. Splitting a group's sole
page can replace that group rather than increase the number of groups. Geometry
is native policy. Splitting may notify selection; a selection veto does not undo
the already committed group structure, so read actual `selection` afterward.

Pinned3.2.12 mouse drag handlers mix group-local indices with global page indices.
To preserve page/selection identity, mouse movement is disabled while multiple
groups exist, and native drag completion is cancelled with native hint/cursor
cleanup. It is restored when removal/movement leaves one group. Use
`move-notebook-page` for checked moves in split notebooks; its remove/insert places
the moved page in the active native group and may remove an empty source group.
Mouse drag splitting (`wxAUI_NB_TAB_SPLIT`) remains disabled; the supported split
operation is the existing native programmatic method, not a constructor policy.

Native private tab handlers in3.2.12 cover501 tab-group IDs per notebook lifetime.
The adapter refuses a split or an empty-book insertion that would allocate beyond
that range; removing groups does not replenish it. Existing pages remain usable,
selectable and removable; create a new notebook to obtain a fresh native range.
This prevents creating private tabs with unusable native click/close handlers.

## Pages, identity and mutation

At most256 pages may be attached. Insert accepts an index from0 through current
count; ordinary page/selection indices must already exist. Captions are copied,
NUL-free strings of at most4096 UTF-8 bytes; empty is valid. Exact native rendering
and mnemonic treatment are port policy. A page must be a fully initialized direct
unsizered PANEL child of that book, with no existing page membership. Native
InsertPage can reparent arbitrary windows, but this binding deliberately refuses
that ownership change. Pages cannot attach themselves during their own initializer.

Page wrappers are strongly rooted before native publication, so callbacks see
canonical identity. An internal book binding retains that canonical identity;
the transport prunes closed-page roots before user CLOSED callbacks, so a
consuming context cannot bypass cleanup. Explicit removal/deletion updates roots
at its Lisp boundary. Cleanup still runs when an earlier callback condition is
pending, without replacing that original condition or calling user handlers.
Callback
errors are re-signalled after native return; successful publication/removal may
already have happened. Moves are staged as native remove/insert while events are
blocked, then restore the selected page by identity. Native allocation failures
are not a general transaction guarantee; a failed move may leave a live detached
page that can be explicitly reinserted.

Remove hides a page without deleting or reparenting it. The returned wrapper
remains a native child of the book, can be reinserted or explicitly destroyed, and
still dies with its native parent. Delete destroys the page and its descendants;
saved wrappers become invalid. Direct destruction of an attached page is rejected.
Destroy the book/ancestor or use the explicit page deletion operation. A row/column
sizer belongs inside a page, not on the book. Managed handler-stack protocols
reject AUI notebooks because their internal manager/tab handlers remain private.

All AUI writers reject Lisp callback use and an active native tab drag/mouse
capture. Finish the tab gesture before retrying a change. Structural
add/insert/remove/delete/move/split
requires a quiescent boundary or the outer owner-bound deferred UI action phase.
Use `defer-ui-action` from menu/button/key/tab callbacks. Ordinary scalar selection
and caption writes remain main-thread operations outside callbacks. Readers may
run in callbacks while lifetimes remain valid. Notifying selection returns its
requested index even if vetoed; read `selection` for actual state. The first page
becomes selected under native rules; exact selection-event counts on insertion,
deletion, unchanged assignments and physical input can vary.

## Native events and close policy

The notify-family native-only event kinds are `:aui-page-changing`,
`:aui-page-changed`, `:aui-page-close`, `:aui-page-closed`, `:aui-drag-done` and `:aui-tab-right-up`.
Bind ordinary observers or current borrowed contexts with `bind-event`.
`event-selection` is native index metadata; `event-old-selection` reports native
old-selection metadata when available. `event-aui-page-id` is an immutable numeric
bridge identity for the captured target, or NIL if unavailable. It does not adopt
or resurrect a page wrapper. The native CLOSED event carries a former index after
removal; its saved target ID is captured during the native close span, never by
looking up that now-shifted index. DRAG-DONE metadata follows native event meaning,
not an application-defined old/new ordering transaction.
Native drag completion can return early, so no exact notification count or full
old/new ordering transaction is promised. Private group-local selection events
are suppressed at the Lisp boundary. Public `event-selection` reports the book's
global catalogue index when that native event defines it; `event-old-selection`
keeps its distinct native semantics and can be group-local for CLOSE.

Payload text/integer/selection/extra metadata and event-ID writers reject for these kinds;
normal scoped Skip/propagation/veto controls retain their contracts. CHANGING and
CLOSE can veto upcoming native actions. CHANGED/CLOSED have already happened, so
veto metadata does not reverse them. A native close emits CLOSE, deletes the page
if allowed, then emits CLOSED. Explicit `delete-notebook-page` is a deletion
command and does not manufacture or promise the user-close veto/event sequence.
Applications can first veto a close, capture its page identity, and defer any
modal decision. Resolve that retained page's current index at delivery before
explicit deletion. Never retain a borrowed context for deferred use.

The bridge guards the complete native tab-button/drag handler span, including
callbacks propagated to ancestors. It protects the book, attached/participating
pages and ancestors against destruction/close; same-book mutation and nested GUI,
modal, clipboard and scheduler entry reject until native work returns. Composite
private child input and native teardown retain Input operation protections.
Owned/synthetic/queued construction and detached cloning of AUI event kinds reject.
Snapshots remain immutable; contexts are valid only in the innermost callback.

## Dynamic splitter extension

The existing fixed two-pane `split-window` remains available. New generic window
operations allow one-pane initialization, explicit unsplit/re-split, replacement
and SETF orientation:

```lisp
(wxlambda:initialize-splitter splitter first-panel)
(wxlambda:resplit-window splitter second-panel)
(wxlambda:replace-splitter-pane splitter 1 replacement-panel) ; returns old panel
(wxlambda:unsplit-window splitter :remove 1)                 ; returns removed panel
(setf (wxlambda:splitter-orientation splitter) :horizontal)  ; or :vertical
```

Initialize requires an empty splitter. Resplit requires exactly one attached pane
and attaches its supplied pane as the second. Replacement addresses an existing
pane0/1. Removed/replaced panels are hidden and retained alive as native children;
they are no longer attached and may be reused or destroyed explicitly. Unsplit
removal0 leaves the former second pane at index0. Parent teardown invalidates all
children, including detached hidden panels. New panes must be fully initialized
distinct direct unsizered PANEL children and cannot already be attached.

These operations reject callbacks and require a safe structural boundary. Roots
are published before native sizing/focus work; old/new pane lifetimes and ancestor
guards cover synchronous native callbacks. Existing positive minimum size,
suppressed double-click unsplit, sash geometry clamping and sash-event semantics
remain. Orientation is now mutable through the generic SETF protocol; it retiles
existing panes and uses native geometry/readback, not exact requested pixels.
Explicit Unsplit does not promise a native sash-triggered UNSPLIT notification;
new resize/double-click/unsplit event families remain unbound.

## Example and verification

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/aui-notebook.lisp \
  --eval '(wxlambda-aui-notebook:start)'
```

The example creates general panel pages through owner-bound deferred Add/Delete/
Move/Split actions inside a two-pane window; tabs can be closed, and native mouse
reordering is available while there is one group.
Tests cover canonical pages, detach/delete distinction, selection veto, callback
deferral, CLOS rollback, stale identities and dynamic splitter panes. Fabricated
Lisp transport tests separately verify cleanup before a consuming CLOSED callback
and while a pending condition skips user callbacks. The native
invariant helper additionally drives the actual private close-button path and
propagated veto, captured CLOSED target identity, synchronous replacement sizing,
foreign-thread rejection, split-group global-index translation, native private-ID
exhaustion guards and teardown. Native rendering, physical drag/focus,
accessibility and hosted GTK/MSW behavior require separate runtime checks. The
binding targets pinned wxWidgets3.2.12 and makes no full wxAUI parity claim.

## Tab context menus

Bind native-only `:aui-tab-right-up` to receive a right-clicked tab's global
`event-selection` and stable `event-aui-page-id`. It does not select the page.
Private tab-control events are filtered until wxAuiNotebook translates them;
resolve the captured page identity and defer popup/structural actions. Payloads
are copied/read-only; owned, synthetic and cloned forms reject as for other AUI
notifications. Physical right-click delivery remains platform-dependent.
