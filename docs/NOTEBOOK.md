# Notebook navigation

ABI37 adds native-backed `notebook` windows with fixed setup-time panel pages:

```lisp
(let* ((book (wxlambda:make-notebook frame))
       (editor (wxlambda:make-panel book))
       (settings (wxlambda:make-panel book)))
  (wxlambda:add-notebook-page book editor :label "Editor λ")
  (wxlambda:add-notebook-page book settings :label "Settings" :select t)
  (setf (wxlambda:selection book) 0))
```

`make-notebook` is a thin generic factory with validated `:class` support. Direct
`make-instance`, subclass slots/default initargs and native properties in subclass
AFTER hooks follow the [CLOS contract](CLOS-API.md). `:parent` is required; ID,
geometry and top-tab style use native defaults. No alternate book class, page icon,
image list, arbitrary notebook style or raw native adoption is exposed.

## Pages and ownership

`add-notebook-page` accepts only an already initialized `panel` whose native parent
is that notebook, with no containing sizer and no existing page membership. Label
defaults to empty; Select defaults NIL. It returns the supplied page wrapper. Add
pages outside RUN, event pumps, modal loops and callbacks (including quiescent
terminal REPL evaluations between pumps). Fully initialized child
pages can be added from a notebook subclass AFTER hook; a page cannot attach itself
while its own initializer is still running. Managed constructor failure cleans up
the notebook/pages and callback roots. Page wrappers are rooted before native
publication, so callbacks during AddPage can use canonical page identity. A callback
condition can escape after adoption; it does not undo a successfully added page.

`notebook-page-count` reads the count and `notebook-page` returns the same EQ panel
wrapper at a zero-based index. Native notebook ownership controls page lifetime.
Direct destruction of attached page panels is rejected; destroying their notebook
or ancestor invalidates them and their descendants. Unattached panel children can
still be destroyed normally. No RemovePage, DeletePage, insertion, reordering,
reparenting or page mutation from running callbacks is provided in this first slice.

Lay out controls inside page panels, and add the notebook to its parent's sizer.
Creating a row/column on the notebook itself is rejected because it manages its
own page layout. Page labels use generic SETF:

```lisp
(wxlambda:notebook-page-label book 0)
(setf (wxlambda:notebook-page-label book 0) "Editor")
```

Strings must be UTF-8 representable without NUL; empty captions are allowed. The
setter returns the supplied string; the getter returns current native caption text.
Native mnemonic processing and caption length limits differ across ports, so exact
arbitrary-label echo is not promised. Indices must be nonnegative signed-int values
within current page count; invalid indices signal instead of native clamping.

## Selection and events

The existing `selection` generic specializes on notebooks. It returns a zero-based
index, or NIL for an empty notebook. The first page's automatic selection follows
the port; adding with Select does not promise an event count or bypass veto.

```lisp
(setf (wxlambda:selection book) 0)           ; native ChangeSelection: silent
(setf (wxlambda:selection book :notify t) 1) ; native SetSelection: vetoable
```

The writer requires an existing integer page index; NIL and `-1` are not accepted
as clearing/sentinel operations. It returns the requested index even if a native
changing handler vetoes; read `selection` afterward for actual state. Silent writes
bypass page-changing events and veto deliberately. Notifying writes use native
events; same-page changes and construction can differ in event counts by port.

Bind `:notebook-page-changing` or `:notebook-page-changed` with `bind-event`. Both
carry immutable snapshots in the notify family, or an exact borrowed event context
with `:with-context t`. `event-selection` reads native wxBookCtrlEvent selection
metadata, distinct from its inherited command integer. Its meaning during CHANGING
is port-dependent: pinned documentation notes Windows may report the old page, so
do not assume a portable proposed-target index or infer it from a control query.
Old-selection metadata is not exposed in this slice. Normal IDs, source/origin,
Skip/propagation, per-binding user data and lifetime rules apply.

```lisp
(wxlambda:bind-event book :notebook-page-changing
  (lambda (event context)
    (declare (ignore event))
    (unless application-allows-navigation-p
      (setf (wxlambda:event-vetoed-p context) t)))
  :with-context t)
```

Veto/Allow metadata is available on both notify events; only CHANGING can prevent
navigation. CHANGED has already happened. Page-event selection metadata is read-only
at the public context level; the choice-event writer does not extend to books.
Synthetic queue/process/owned construction, contextual cloning, and detached page
events are rejected because they require the actual derived native class. Observer
bindings Skip automatically; contextual bindings choose Skip explicitly.

## Reentry and verification

Bridge navigation, AddPage and actual native page-event callbacks (including
propagated bindings) protect the notebook and its ancestors. Same-notebook recursive
selection, direct destruction/close of protected receivers/ancestors, and nested
modal or clipboard entry are rejected until the operation/callback returns. Defer
prompts/transfers to later application work; callback-time event pumping is already
forbidden. Different notebook scalar operations can nest under their own guards.
Callbacks emitted during native teardown are suppressed for a deleting origin.
Uncaught callback conditions re-signal at the surrounding Lisp boundary after native
code returns; native selection/adoption may already have completed. No rollback or
exact post-error native selection outcome is promised; later silent selection can
recover a usable notebook.

`examples/notebook.lisp` builds an editor/settings notebook, a navigation-veto
checkbox and a Back button. START is interactive; EXERCISE uses real native
selection, veto and canonical page lookup:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/notebook.lisp --eval '(wxlambda-notebook:start)'
sbcl --load ~/quicklisp/setup.lisp --load examples/notebook.lisp --eval '(wxlambda:with-application () (wxlambda-notebook:exercise))'
```

The Lisp regression covers subclass construction/defaults, setup-time publication,
Unicode captions, selection/veto/silent behavior, invalid ownership/indices, attached
page and layout guards, callback errors, initializer rollback and invalidation.
The native harness dispatches an actual derived wxBookCtrlEvent outside bridge
selection guards and through parent propagation, checking lifetime/reentry/modal/
clipboard protection. Physical tab clicks, keyboard navigation, port-specific
caption/CHANGING metadata behavior and hosted Windows/Linux remain separate checks.
See [coverage](WXWIDGETS-COVERAGE.md), [event contexts](EVENT-CONTEXTS.md) and
[Cadre installation](DOCSET.md).
