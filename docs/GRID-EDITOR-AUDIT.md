# Pinned wx3.2.12 grid editor/event audit

Read-only source survey before the next bounded implementation. Sources:
[grid.cpp](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/generic/grid.cpp),
[grideditors.cpp](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/generic/grideditors.cpp),
[gridsel.cpp](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/generic/gridsel.cpp)
and [grid interface](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/grid.h).
Copies are in /private/tmp/wxl-grid-{grid.cpp,grideditors.cpp,gridsel.cpp}.

## Commit, hide and reset are distinct native operations

| Native operation | Observed pinned behavior |
| --- | --- |
| SaveEditControlValue | If enabled, EndEdit computes proposed value, CELL_CHANGING can veto, ApplyEdit updates the table, then CELL_CHANGED reports old value. Leaves editor enabled/shown; not a transaction boundary. |
| DisableCellEditControl | Sends EDITOR_HIDDEN, then DoAccept sets enabled=false, hides and saves through the same validation path. HIDDEN veto does not stop it. This is a commit attempt, not cancel. |
| HideCellEditControl | Calls DoHide only and does not directly save or disable. It may refocus grid; editor KillFocus calls DismissEditor, which queues DisableCellEditControl with CallAfter. Consequently Hide can cause a deferred commit and cannot be advertised as cancellation. |
| Native editor Reset | The Escape handler calls editor.Reset then deferred Disable. For the native text editor, Reset restores its current internal m_value baseline, not necessarily the original table value across prior Save/veto calls. |

Text editor EndEdit assigns m_value before validation. ApplyEdit clears it after
successful table write. Therefore repeated Save can emit repeated native change
events, and after a veto the editor's own baseline may differ from the table.
Do not invent a transaction API or promise cancellation/validation retry behavior
that upstream methods do not supply. A bounded current-editor Reset adapter must
identify the native operation honestly and retain no editor resource/pointer.

## Events and veto semantics

CELL_CHANGING GetString is proposed new text; GetCellValue is still current old
text. CELL_CHANGED GetString is old text; GetCellValue is current applied text.
Native legacy CELL_CHANGED veto restores the old table value after callbacks;
new validation should use CHANGING. Current value must be copied at snapshot
delivery and remain independent from the mutable live table.

SELECT_CELL occurs before cursor movement and can veto. Its row/column are the
proposed target; current grid cursor is still previous. No corresponding native
post-cursor-change event exists in this pinned interface. RANGE_SELECTING and
RANGE_SELECTED report changing/final blocks, but native selection code ignores
their IsAllowed result, so their veto is observational. Keyboard selection may
emit SELECTED without SELECTING. SHOWN can veto before editor activation; HIDDEN
is unconditional and may be sent even when editor creation failed.

All events use existing BIND-EVENT, immutable snapshots/scoped contexts, and
actual-origin/composite lifetime guards. No synthetic/owned/cloned grid events,
mutable row/column/payload metadata, custom table/model callbacks, or retained
native editor/attribute pointers. Do not infer transaction success from generic
notify CanVeto metadata alone.

## Proposed bounded scope

- General native cell/change/select/range/editor/click kinds160..170, numeric
  row/column/range/modifier metadata and copied native text/current cell value.
- Native cursor and editor enabled/shown/read-only capability queries; guarded
  Save/Hide/Disable (and optional bounded current-editor Reset) outside callbacks.
- Per-cell read-only, background and text foreground properties using owned
  native attributes and packed RGB values, no attribute resource adoption.
- Native string-table Append/Insert/Delete rows/columns under existing active
  editor and main-thread guards; dimensions bounded0..256 after deletion, at
  most16384 cells, while construction may remain nonempty. Zero dimensions
  require NIL cursor/sentinel-aware reads and CanEnableCellControl checks.

Structural writes require safe quiescence/owner-bound UI action. Active-editor
property/table changes reject. Explicit native editor operations retain full
InputOperation source/ancestor/native-entry guards around synchronous callbacks
and focus work. Native table changes may notify differently by operation; do not
manufacture CELL_CHANGING/CHANGED for silent SetCellValue calls.

## Acceptance probes

Use actual default text editor in native fixtures, changing its private control
only for tests. Verify proposed/old/current snapshot values, CHANGING veto,
legacy CHANGED rollback, SHOWN veto/HIDDEN observation, Save enabled state,
Hide deferred focus-loss behavior, and Reset-before-Disable baseline semantics.
Cover selection target/range payloads and context expiry, actual/composite/parent
callback lifetime guards, table mutation bounds/empty states/attributes moving
with native rows, active-editor rejection and native teardown. Both-Lisp examples
must avoid physical-input claims; no system clipboard transfer in automation.
