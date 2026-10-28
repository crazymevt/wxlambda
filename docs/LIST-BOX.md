# Single-selection list boxes

ABI38 adds `list-box`, an unsorted native list with at most one selected row:

```lisp
(wxlambda:make-list-box parent :items '("Ada" "Grace λ" "日本語 😀") :selection 1)
(make-instance 'wxlambda:list-box :parent parent :items '("" "duplicate" "duplicate"))
```

The parent is required. Items default to an empty finite proper list of strings;
selection defaults NIL (none). Empty strings and duplicates are accepted. Strings
cross an explicit-length UTF-8 boundary and reject NUL. A selected index must exist.
The thin generic factory validates `:class` and forwards subclass slots/initargs.
Native-backed direct construction, default initargs, native properties in AFTER
hooks and failed-initialization cleanup follow the [CLOS API](CLOS-API.md).
No sorting, multi/extended selection, checkboxes, owner drawing, virtual models,
item client data or arbitrary style flags are exposed in this slice.

## Items and selection

```lisp
(wxlambda:item-count control)
(wxlambda:item-text control 0)            ; independent string
(setf (wxlambda:item-text control 0) "Ada λ") ; returns supplied string
(wxlambda:append-item control "Another")  ; appended zero-based index
(wxlambda:delete-item control 0)          ; T
(wxlambda:clear-items control)           ; T
(wxlambda:selection control)             ; index or NIL
(setf (wxlambda:selection control) nil)   ; silent deselection
(setf (wxlambda:selection control :notify t) 0)
```

Item operations currently specialize on LIST-BOX only; no receiver fallbacks or
CHOICE expansion is added. All operations require an active main-thread session
and valid native receiver. Indices are signed-int nonnegative integers within
current count; invalid indices signal instead of native clamping. Setf selection
also accepts NIL to clear. Native failures signal `native-error`; Lisp dispatch/
type errors remain Lisp conditions. Native failures are not transactional rollback
of earlier side effects.

Append preserves selection and returns its new index. Rename preserves the selected
index. Delete clears selection when deleting that row; deleting an earlier row
shifts the selected index down one, preserving the same item. Clear leaves no items
or selection. These item mutations and default selection SETF suppress selection
events. They are allowed from list callbacks; a temporary native event blocker
restores any managed pushed-handler stack before returning. `:notify t` selection
SETF changes the model then emits one synthetic native selection event, including
unchanged assignments and NIL. It returns the supplied index/NIL. A callback error
can escape after the selection changed; handled errors permit subsequent use.

Indices describe current order, not durable identities. Saved indices/snapshots can
refer to different rows after edits. Keep stable IDs and domain data in Lisp models
when needed; strings, duplicates and native item positions are not an ID system.
Native display, scrolling and keyboard behavior remain port-specific.

## Selection and activation events

`on-selection`/`off-selection` specialize on LIST-BOX for one replaceable convenience
observer. It receives `:listbox-selection`, distinct from CHOICE's `:selection`.
Generic `bind-event` supports both `:listbox-selection` and `:listbox-double-click`
(including native keyboard activation when the port emits it):

```lisp
(wxlambda:bind-event control :listbox-double-click
  (lambda (event)
    (format t "Activated index ~S, text ~S~%"
            (wxlambda:event-selection event) (wxlambda:event-text event))))
```

Both are command events with copied string/index payloads, origin metadata,
propagation and per-binding Lisp data. `event-selection` is index/NIL; `event-text`
is the native event's cached label (empty for a synthetic NIL selection). Current
widget state may already differ after a preceding callback edits items. Observers
Skip automatically; contextual callbacks explicitly control Skip. Snapshots remain
immutable. Context/owned `event-selection` and `event-text` writes change event
payload only, not the list model. Raw `event-selection-p`/extra-LONG action metadata
is independent of the index after metadata mutation; bridge-generated list events
initialize it from whether a selection was supplied. It is not a veto protocol.

Unlike notebook derived events, these simple wxCommandEvent kinds support owned
construction, cloning, queue/process/post/transfer and standalone/non-list receivers.
Synthetic events never change list items/selection automatically. `:text` and
`:selection` are accepted for these kinds. Native actual selection/double-click
callbacks protect the originating list and its ancestors, including propagated and
convenience callbacks. Direct window destruction/close and synchronous modal or
clipboard entry are rejected until callbacks return; defer those actions to later
application work. Item mutations and nested selection are allowed. Native callbacks
from deleting origins are suppressed. This prevents a nested native loop from
pumping deletion while the port's activation stack still uses the widget.

## Example and checks

`examples/list-manager.lisp` wires a list, edit field and Append/Rename/Delete/Clear
buttons, plus selection and activation feedback. START is interactive; bounded
EXERCISE verifies real native item/selection behavior without physical input:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/list-manager.lisp --eval '(wxlambda-list-manager:start)'
sbcl --load ~/quicklisp/setup.lisp --load examples/list-manager.lisp --eval '(wxlambda:with-application () (wxlambda-list-manager:exercise))'
```

Lisp tests cover direct/factory subclasses/defaults/AFTER hooks/rollback, Unicode,
empty/duplicate strings, copied storage, deterministic selection adjustment, silent
edits, managed handler stacks, context edits/snapshots, detached/synthetic events and
callback errors. The native harness dispatches actual list events outside the setter
through generic, propagated and convenience callbacks, probing lifetime/native-loop
guards and item mutation. Raw invalid UTF-8/NUL/arrays/indices/pointers and stale or
wrong receivers reject without wx assertions. Physical mouse/keyboard activation,
scrolling/accessibility, hosted Windows/Linux and a complete tree/list-manager app
remain separate checks. See [coverage](WXWIDGETS-COVERAGE.md),
[event contexts](EVENT-CONTEXTS.md) and [Cadre installation](DOCSET.md).
