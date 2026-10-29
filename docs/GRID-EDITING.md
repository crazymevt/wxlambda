# Native grid editing, selection and table changes

This slice extends the owned `wxGridStringTable` created by `GRID`. It adds native
editor operations, cell attributes, selection events and dimension changes. It
does not expose custom table models, editor ownership or a transaction API.

`GRID-CELL-READ-ONLY-P`, `GRID-CELL-BACKGROUND` and `GRID-CELL-FOREGROUND` are
generic SETF properties with `(grid row column)` readers. Colours are copied
integers `#xRRGGBB`. Coordinates are zero based. The global `GRID-EDITABLE-P`
switch and individual read-only cells both affect native editor availability.
`GRID-CAN-ENABLE-CELL-CONTROL-P` queries native availability at the cursor;
`GRID-CELL-EDITOR-SHOWN-P` distinguishes visibility from `GRID-EDITING-P`.

`GRID-CURSOR` returns a fresh `(row column)` pair or NIL for an empty table.
SETF requests native `SetGridCursor`, which can be vetoed by SELECT_CELL; read
back to learn the resulting cursor. `SELECT-GRID-BLOCK` takes grid, top, left,
bottom and right, with optional `:ADD`; `CLEAR-GRID-SELECTION` clears native
selection. There is no added application selection model.

## Editor operations preserve native distinctions

All operations take a grid and return it. They must run after callbacks return,
normally through an owner-bound `DEFER-UI-ACTION`.

| Lisp generic | Native method and behavior |
| --- | --- |
| ENABLE-GRID-CELL-EDIT-CONTROL | EnableCellEditControl; rejects when CanEnableCellControl is false. |
| SAVE-GRID-EDIT-CONTROL-VALUE | SaveEditControlValue; attempts validation and table update, leaving the editor enabled. Repeated calls can produce repeated native notifications. |
| HIDE-GRID-CELL-EDIT-CONTROL | HideCellEditControl; hides the editor and leaves it enabled. Focus loss can schedule a later native Disable and save. It is not cancellation. |
| DISABLE-GRID-CELL-EDIT-CONTROL | DisableCellEditControl; sends HIDDEN and attempts to save. It is a commit attempt, not cancellation. |
| RESET-GRID-CELL-EDITOR | Reset on the active created native text editor, held through a temporary reference-counted GetCellEditor result. It restores that editor's current baseline. |

For an unchanged baseline, Reset followed by Disable discards newly typed text.
Reset does not guarantee restoration of the original table value after a prior
Save or veto: the native text editor updates its internal baseline during
EndEdit. There is no general cancel guarantee. Normal native Escape behavior
also performs Reset followed by deferred Disable.

Cell value, attribute and dimension writes reject while an editor is enabled.
Finish the native edit first. Native editor operations and cursor/selection
changes retain actual-source and ancestor lifetime guards through synchronous
events. Active callbacks reject destruction, nested native loop entry and
mutations; read-only native properties remain available.

## Native event families

These immutable snapshots and borrowed current callback contexts use `BIND-EVENT`:

| Keyword | Native meaning |
| --- | --- |
| :GRID-CELL-CHANGING | Proposed text is EVENT-TEXT. Veto prevents ApplyEdit. |
| :GRID-CELL-CHANGED | EVENT-TEXT is the old text. Legacy native veto restores it after callbacks. |
| :GRID-SELECT-CELL | Before cursor movement; native veto can prevent it. No post-cursor event is invented. |
| :GRID-RANGE-SELECTING / :GRID-RANGE-SELECTED | Native range notifications; veto is observational because the audited native sender ignores it. Some keyboard paths emit only SELECTED. |
| :GRID-EDITOR-SHOWN | Before native editor creation/show; veto can prevent it. |
| :GRID-EDITOR-HIDDEN | Native notification; veto does not prevent hide/save. Creation failure can also emit it. |
| :GRID-CELL-LEFT-CLICK / :GRID-CELL-RIGHT-CLICK | Native cell mouse notifications. |
| :GRID-CELL-LEFT-DOUBLE-CLICK / :GRID-CELL-RIGHT-DOUBLE-CLICK | Native cell double-click notifications. |

`EVENT-GRID-ROW`, `EVENT-GRID-COLUMN` and `EVENT-GRID-POSITION` apply to cell
events. Position is a fresh `(x y)` in grid client coordinates including label
offsets, or NIL for native `(-1 -1)`. `EVENT-GRID-RANGE` applies to range events
and returns fresh `(top left bottom right)`. `EVENT-GRID-MODIFIERS` and
`EVENT-GRID-SELECTING-P` apply to both. Values are native metadata, not inferred
application state. Invalid kind/accessor combinations signal an error.

`EVENT-GRID-CELL-VALUE` is limited to CHANGING/CHANGED. Snapshots copy the table
value before the user callback; contexts copy the current table value during
their current innermost callback. CHANGING therefore ordinarily sees the old
value while EVENT-TEXT contains proposed text; CHANGED sees applied text while
EVENT-TEXT contains old text. Returned strings are independent copies. A legacy
CHANGED veto can subsequently roll back the table, so snapshots do not predict
the final table. No native pointers or mutable event resources are exposed.

Default observers automatically Skip. `:WITH-CONTEXT T` permits explicit Skip,
propagation and scoped native notify veto. An explicitly consumed event can
prevent default native mouse behavior; use ordinary observers when preserving
selection/editing. Event metadata/text remain read-only; detached creation,
synthetic dispatch and cloning reject these kinds.

## Dynamic owned table dimensions

`APPEND-GRID-ROWS` and `APPEND-GRID-COLUMNS` take optional count (default 1).
`INSERT-GRID-ROWS`, `DELETE-GRID-ROWS`, `INSERT-GRID-COLUMNS` and
`DELETE-GRID-COLUMNS` take a zero-based position and optional count. Native
methods shift cell data, labels and attributes. Each count is 1..256; resulting
axes are 0..256 with at most 16384 cells. Constructor axes remain 1..256.
An empty axis makes cell access invalid; append/insert can restore cells.
Dimension edits require the safe structural phase outside callbacks, RUN and
event pumps, or an owner-bound deferred UI action. All table operations return
the grid and signal on native failure; there is no custom-model ownership.

See [the pinned editor audit](GRID-EDITOR-AUDIT.md) for wxWidgets 3.2.12 source
links and detailed commit/hide/reset timing. The standalone example is
`examples/grid-editing.lisp`; run its START function under SBCL or CCL.
