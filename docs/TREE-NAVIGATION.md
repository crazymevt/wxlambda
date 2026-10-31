# Trees and managed item views

ABI39 adds a visible-root, single-selection `tree-control` and borrowed `tree-item`
views. Items use checked bridge tokens, never user-provided raw `wxTreeItemId`
pointers. Native token entries retain owner/parent/child relationships; Lisp views
are canonical within the session:

```lisp
(let* ((tree (wxlambda:make-tree-control parent))
       (root (wxlambda:add-tree-root tree "Workspace λ"))
       (folder (wxlambda:append-tree-item root "Documents"))
       (file (wxlambda:append-tree-item folder "日本語 😀.txt")))
  (setf (wxlambda:tree-item-expanded-p root) t)
  (setf (wxlambda:tree-item-expanded-p folder) t)
  (setf (wxlambda:selection tree :notify t) file))
```

TREE-CONTROL uses native-backed CLOS construction, required `:parent`, validated
`:class` factory support and subclass/default-initarg/AFTER-hook rollback. Fixed
styles provide a visible root, branch buttons/root lines and single selection.
TREE-ITEM is a borrowed resource view, not a window or directly constructible class;
obtain it from creation, traversal, selection or event APIs. The wxQt tree backend is unavailable: construction signals `native-error` before
allocation because its asynchronous raw-item events need generation-safe translation.
This slice targets the reviewed synchronous generic/macOS, GTK and MSW paths;
hosted GTK/MSW still need runtime verification. No item subclass factory,
raw adoption, native item data, images, sorting, label editing, drag/drop, hidden root,
multiselect, lazy/virtual loading or arbitrary styles are exposed.

## Ownership and traversal

`add-tree-root` requires an empty tree; `append-tree-item` accepts a live managed
parent and appends in native order. Both take a string and return a TREE-ITEM view.
Empty/duplicate labels are valid. Explicit UTF-8 lengths reject NUL. `tree-root`
returns the root view or NIL. `tree-item-parent` returns its canonical parent or NIL
for root; `tree-item-children` returns a fresh ordered list of canonical views.
Repeated queries return EQ views. Native insertion blocks supported selection events
until the token and Lisp view can be published; failed Lisp publication deletes the
new native item. No structural edit is allowed on the same tree during its selection
callback. Edits from separate button callbacks or quiescent evaluations are supported.

```lisp
(wxlambda:tree-item-text item) ; independent string
(setf (wxlambda:tree-item-text item) "Renamed λ")
(wxlambda:tree-item-expanded-p item)
(setf (wxlambda:tree-item-expanded-p item) t)
(wxlambda:tree-item-alive-p item)
(wxlambda:delete-tree-item item) ; T; deletes the entire subtree
```

Mutable properties use generic SETF and return the supplied value. Expansion is a
native request; a leaf may not become expanded, so query actual state if needed.
All native item operations validate token and owner before accessing the opaque
native ID. Cross-tree selection, deleted items and invalid receivers reject.
`tree-item-alive-p` returns NIL for invalidated/deleting-owner items within an active
session; it signals outside the session like other native queries.

Subtree deletion invalidates every descendant before native deletion. Native delete
notifications and early control-destructor cleanup also invalidate tokens. Native
ID/address reuse cannot revive a stale view. The session cache is revision-pruned
after deletion/normal event boundaries and cleared at session cleanup; managed
initialization rollback prunes invalid views too. Retaining a view keeps its Lisp
owner reference, not the deleted native item. Native tree/ancestor destruction
invalidates all items, including views held in user data or snapshots.

## Selection policy

`selection` returns a TREE-ITEM view or NIL, not an integer index. Root creation
silently selects the new root; append preserves current selection. A nonempty tree
selection writer requires a live item from that same tree. NIL is accepted only
for an empty tree as a no-op. This avoids generic wx tree idle auto-selection after
native deselection. Public mutations normalize selection synchronously under native
event blockers so idle processing cannot introduce delayed wrapper-induced changes:

- Deleting a subtree preserves an outside selection, or selects the surviving parent/root.
- Deleting the root leaves an empty, unselected tree.
- Collapsing a branch containing selection promotes selection to that branch.
- Other expansion changes preserve current selection.

Text, insertion, deletion and expansion operations suppress supported selection
events. Default selection SETF is silent. `:notify t` uses native `SelectItem`, with
native changing/changed behavior and veto; same-item event counts are port-dependent.
The writer returns its requested item even if vetoed; query actual selection.
Physical user interaction follows the native port's navigation behavior.

## Native events and lifetimes

`:tree-selection-changing` and `:tree-selection-changed` are derived wxTreeEvent
notify events. Bind them through `bind-event`, using a current context for veto:

```lisp
(wxlambda:bind-event tree :tree-selection-changing
  (lambda (event context)
    (let ((item (wxlambda:event-tree-item event)))
      (when (and item application-blocks-navigation-p)
        (setf (wxlambda:event-vetoed-p context) t))))
  :with-context t)
```

`event-tree-item` reads the associated canonical view from a snapshot or exact live
tree context, or NIL when no managed item is associated. It does not expose an old
item or native pointer. Non-tree contexts reject; non-tree snapshots return NIL.
There is no owned-event method or setter. `event-selection` remains integer metadata
for other controls and returns NIL for tree events. Only CHANGING veto prevents
navigation; CHANGED is already complete. Source/origin, Skip/propagation, per-binding
Lisp data and contained error behavior follow existing context contracts. No tree
convenience `on-selection` method is added; use generic bindings.

Snapshots freeze **view identity**, not item contents or validity. A saved event may
return an invalidated TREE-ITEM after deletion. Check liveness before using it, and
copy `tree-item-text` while live if a frozen label is needed. Snapshot access itself
needs no session; dereferencing a view still requires one. Native event item metadata
can be absent, and no unmanaged native item is adopted automatically.

Tree operation and actual/propagated callback guards protect the tree and ancestors.
Same-tree writes (including item edits/selection), widget destruction/close, event
pumping, synchronous modal entry and clipboard entry must wait until callbacks
return. Reads and veto are allowed. Native teardown callbacks are suppressed even
inside the native base destructor. Callback conditions re-signal after native return;
completed native mutations are not rolled back. Derived tree events cannot be
synthesized, queued as owned events, detached or contextually cloned in this slice.
The appended ABI snapshot token field requires the matching ABI39 bridge/FFI pair.

## Example and checks

`examples/tree-navigation.lisp` builds an expandable document tree, a navigation
veto checkbox and buttons to append/delete subtrees outside selection callbacks.
START is interactive; bounded EXERCISE verifies native veto, canonical navigation
and subtree invalidation:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/tree-navigation.lisp --eval '(wxlambda-tree-navigation:start)'
sbcl --load ~/quicklisp/setup.lisp --load examples/tree-navigation.lisp --eval '(wxlambda:with-application () (wxlambda-tree-navigation:exercise))'
```

Lisp tests check CLOS/defaults/rollback, views/text, ownership, post-idle collapse/
delete normalization, snapshots after deletion, cache pruning, veto/errors and stale
or cross-tree operations. Native probes dispatch real derived events outside bridge
selection setters and through parent propagation, then check mutation/lifetime/
nested-loop guards, raw inputs, native deletion, token reuse protection and base
teardown suppression with zero wx assertions. Physical mouse/keyboard navigation,
large/deep/virtual trees, accessibility, hosted Windows/Linux, graphical Cadre and
a complete file/tree manager remain separate verification. See [coverage](WXWIDGETS-COVERAGE.md),
[event contexts](EVENT-CONTEXTS.md) and [Cadre installation](DOCSET.md).

Multi-selection clearing and deletion normalize actual selected items even when
the native Windows tree has no focused caret. This avoids wx3.2.12
`UnselectAll` returning early and retaining an unrelated selection.
