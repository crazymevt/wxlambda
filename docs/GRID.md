# Fixed-shape editable grids

The native-backed `grid` WINDOW subtype exposes a useful bounded `wxGrid` string
table. It supports native mouse/keyboard cell selection and editing, copied cell
text, row/column labels and whole-grid editing policy. It is a string table: there
is no formula evaluation, numeric coercion, custom table, editor, renderer or
attribute ownership API.

```lisp
(defparameter table
  (wxlambda:make-grid parent :rows 12 :columns 3 :editable-p t))
(setf (wxlambda:grid-column-label table 0) "Item"
      (wxlambda:grid-row-label table 0) "First"
      (wxlambda:grid-cell-text table 0 0) "Keyboard λ")
(wxlambda:grid-cell-text table 0 0) ; fresh Lisp string
(wxlambda:grid-rows table)         ; 12
(wxlambda:grid-columns table)      ; 3
(wxlambda:grid-editable-p table)    ; T
(wxlambda:grid-editing-p table)     ; native cell editor currently enabled?
```

`:parent` requires a live WINDOW. Defaults are10 rows,5 columns and editable T.
Each dimension is1..256 and the total cannot exceed16384 cells. Shape is fixed;
there are no row/column insertions/deletions or custom table adoption. Cell indices
are zero-based and checked against the actual shape. Labels initially use native
defaults; applications can replace them with strings, including empty strings.
Programmatic cell/label text is NUL-free and at most65536 UTF-8 bytes per value.
Multiline strings are accepted. Reads copy native text with explicit lengths;
changing the returned Lisp string never changes the grid. The programmatic limit
does not constrain text entered through native editing.

`grid-cell-text`, `grid-row-label`, `grid-column-label` and `grid-editable-p` have
generic SETF writers returning the supplied value. Editing state accepts generalized
booleans. Turning editing off prevents native user changes; it does not prohibit
programmatic cell text updates. The initial edit policy does not imply every
future cell will have arbitrary editor or per-cell read-only overrides.

All grid writes reject Lisp callback use, including writes to another grid. Use
an owner-bound `defer-ui-action`. Writes also reject while the native cell editor
is enabled, even if currently hidden or the requested value is unchanged. This
avoids implicitly committing an unfinished edit or hiding/recreating its editor.
Finish or cancel the edit with the native keyboard/mouse interaction first.
`grid-editing-p` reports editor enablement, not focus or unsaved-change state.
Readers remain callback-safe and report committed table values; an unfinished
editor buffer can differ. There is no explicit editor commit/cancel command.

The wrapper uses the native-owned default `wxGridStringTable`; native editors,
renderers, selection state and composite child windows remain private. There are
no public grid event types or immutable grid event snapshots, selection/cursor
properties, sorting, validation/veto hooks, custom callbacks or spreadsheet
clipboard operations. Generic child TEXT/BUTTON events retain their existing
event scope and are not a grid cell-change API. Inherited window operations,
including layout/geometry/visibility, are available under their normal contracts;
managed handler stacks remain unavailable for this native scrolled composite.

All operations require the active main-thread GUI session and checked native
lifetime. Parent destruction invalidates the grid. Composite child input callbacks
protect the grid and its ancestors from destruction/close, and prevent nested GUI,
modal or clipboard entry. Mutations hold the shared Input operation guard; early
Destroy and native-base teardown suppress child input dispatch while the native
editor/table is being removed. CLOS construction supports direct subclasses,
slot/default initargs, native-ready AFTER hooks, validated factory `:class` and
managed initializer failure/root rollback. Lisp subclassing does not override
native C++ virtual methods.

Requires `wxUSE_GRID=1` in the pinned wxWidgets3.2.12 core build. The C ABI constructor
returns a handle or0; property getters return nonnegative values or-1 on error;
string readers return borrowed UTF-8 plus explicit length orNULL on error; setters
return0/-1. Lisp copies strings before another native call and maps errors to
`native-error`.

Run the example with:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/grid.lisp \
  --eval '(wxlambda-grid:start)'
```

The example displays an inventory table and reads a committed value through an
owner-bound deferred button action. `tests/grid.lisp` exercises its automatic
path, copies/bounds, CLOS rollback, callbacks and disposal. The native invariant
helper also exercises a real string editor, private child TEXT/BUTTON propagation,
active-editor refusal, synchronous operation reentry, early Destroy restoration
and foreign-thread rejection. Native interaction, rendering, accessibility and
editing details remain port policy; local automated checks do not establish
cross-platform manual interaction fidelity.
