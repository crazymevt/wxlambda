# Cohesive tree/list record manager

`examples/record-manager.lisp` composes existing bindings into one usable manager:
category tree, search input, single-selection record list, splitter panes,
Records/Details notebook pages, read-only multiline details, menu and toolbar
actions, portable accelerator IDs, generic dialogs and clipboard copy. It adds
no native binding or public library export. The records stay in memory;
persistence and external database integration are application work.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/record-manager.lisp \
  --eval '(wxlambda-record-manager:start)'
```

Select Documents or Images, filter titles, select a row, and use New/Edit/Delete/
Copy record/Details through the menu, toolbar or configured keyboard shortcuts.
Copy record uses the system text clipboard only in the interactive START path.
New and Edit use two sequential text-entry prompts; Delete uses a generic
Yes/No message dialog. Cancel leaves model records unchanged. A nonempty
single-line title is required; NUL is rejected in both fields. Notes can be
multiline in the model and details view, although the bounded native prompt
used here is single-line. The example does not advertise rich editing.

## Model and callback behavior

Records have stable numeric IDs, category keywords, copied title and note
strings. List indices are a temporary view over visible IDs. The category tree's
borrowed item views map to category keywords; they are not stored as record IDs.
Filtering uses Lisp CHAR-EQUAL substring matching, not locale/Unicode collation.
Changing category or query preserves a selected record only if its ID remains
visible; a hidden/deleted selection clears and disables selection-only actions.

Native tree/search callbacks enqueue one owner-bound deferred refresh. Pending
refreshes coalesce and read the latest category/query at delivery. An explicit
cancel request merges a clear flag into any pending refresh; clearing at delivery
is application policy, not an assumption about native cancel timing or payload.

Menu, toolbar and keyboard commands converge on the same deferred model action.
Contextual menu bindings consume commands so toolbar/frame propagation does not
queue duplicate work. The first pending CRUD/copy/details request wins. New
requests while an action is active are ignored. Action targets and creation
categories are captured as Lisp IDs/keywords at request time, avoiding accidental
retargeting when row order, filters or navigation change before delivery. The
record's current contents are resolved from that ID when the action runs.
Details refuses to resurrect a target hidden by an earlier pending refresh;
deleting a captured target preserves a later selection of another surviving ID.
Native command checked/enabled metadata is not the application model.

All modal/clipboard entry and notebook navigation happen in the safe idle phase.
The action-active flag is cleared on normal return or unwinding. Unexpected
adapter conditions propagate with their identity intact; a failing prompt does
not partially mutate records. Explicit disposal cancels pending actions/refreshes
before requesting frame destruction. Native owner destruction also cancels work
through the existing weak-owner mechanism. This does not turn frame close
acceptance into a native destruction guarantee.

## Deterministic verification and scope

`EXERCISE` runs inside an active application session. It injects prompt,
confirmation and clipboard functions, so automatic tests never read or overwrite
system clipboard content and never require manual modal input. It checks:

- category navigation and Unicode filtering across tree/list views;
- menu/toolbar/synthetic command convergence and deferred execution;
- first-request-wins creation, stable-ID edit/copy targets and details navigation;
- create/edit/delete success, canceled edit/delete and invalid title refusal;
- selected-row/model/detail/menu/tool state synchronization.

`tests/record-manager.lisp` adds pending cancel-refresh merging, NIL selection,
action reentry refusal, original prompt-condition identity, no partial mutation,
active-flag cleanup, and cancellation before owner disposal. The synthetic menu
command checks action logic, not a physical keypress. Native modal and clipboard
bindings retain their independent fixture/isolation tests. Physical pointer/key/
IME interaction, hosted GUI ports, native clipboard interoperability and graphical
Cadre remain unverified by these workflow tests.

## Finite primary-goal checkpoint

The project's primary goal is broad everyday GUI coverage with selective depth,
not exhaustive wxWidgets parity. For a local verified macOS MVP, a finite stopping
point is acceptance of these representative workflows plus the shared reliability
and delivery gates:

| Workflow | Evidence required |
| --- | --- |
| Form/editor | Existing persistent document form: open/save/cancel/error/dirty close behavior, menus/tool/keyboard actions, clipboard through isolated adapters, multiline text and navigation primitives. |
| Tree/list manager | This integrated example: category/filter/selection/model IDs, CRUD dialogs/actions, details, cancellation/failure/disposal and clipboard adapter acceptance. |
| Drawing/monitoring | A retained drawing canvas with bounded geometry, colour/font/text and bitmap adapters, resize/DPI scope, and owner-bound periodic updates; see [drawing/monitoring](DRAWING-CANVAS.md) and the consolidated [acceptance report](MVP-ACCEPTANCE.md). |
| Shared delivery gates | Native/main-thread/lifetime/event regressions; both-Lisp suite and package checks; all exports documented; guides/examples and public references discoverable in a zero-problem Cadre-readable docset; independent review resolved. |

Once all rows pass on the declared local target, further widgets and native
inventory entries are optional expansion and should not be treated as an endless
primary-goal backlog. Hosted Windows/Linux/Qt, physical interaction and graphical
Cadre are separate unverified acceptance targets and must not be advertised as
completed. A local milestone declaration must name that boundary explicitly.

See [feature policy](WXWIDGETS-COVERAGE.md), [deferred actions](DEFERRED-UI-ACTIONS.md),
[tree navigation](TREE-NAVIGATION.md), [search controls](SEARCH-CONTROLS.md) and
[Cadre installation](DOCSET.md).
