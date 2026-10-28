# Toolbar and status-bar actions

ABI41 supplies frame-owned `toolbar` and `status-bar` window subclasses.
Use native-backed `make-instance` with `:parent frame`, or thin generic factories
`make-toolbar` / `make-status-bar` with validated `:class`. Subclass slot initargs,
default initargs and AFTER hooks work with native properties ready. Failed managed
initialization cleans up native children and callback roots.

A frame owns at most one of each bar. Create/destroy bars outside RUN, pumps,
callbacks and modal loops. Direct bar destruction is synchronous at quiescence,
clears frame associations and permits replacement. Destroying a frame invalidates
its bars. Bars manage frame chrome: do not install sizers on them or add them to
frame sizers. Lay out ordinary content in a panel or the frame client area.

## Fixed toolbar tools

```lisp
(let ((bar (wxlambda:make-toolbar frame)))
  (wxlambda:add-toolbar-tool bar 1001 "Open" :art :open :help "Open a document")
  (wxlambda:add-toolbar-separator bar)
  (wxlambda:add-toolbar-tool bar 1002 "Details" :kind :check :art :information)
  (wxlambda:realize-toolbar bar))
```

Tool IDs are positive signed 32-bit integers, unique within a toolbar. They may
match menu action IDs intentionally; no global menu/tool state synchronization
is implicit. Tools are addressed by toolbar plus ID, with no borrowed tool wrapper
or raw native tool adoption. Labels are nonempty strings; help defaults to empty.
Text uses explicit UTF-8 lengths and rejects embedded NUL.

Supported `:kind` values are `:normal` and `:check`. Stock `:art` values are `:new`,
`:open`, `:save`, `:copy`, `:paste`, `:quit` and `:information` (default). The native
art provider is asked for 16-by-16 bitmaps (native DPI behavior applies); unavailable art signals NATIVE-ERROR.
Separators have no public action ID or state. `toolbar-tool-count` includes them.
Add tools/separators only while quiescent before the one successful
`realize-toolbar`; it returns T and rejects an empty/already-realized bar.
Realization fixes the tool collection. No removal, reordering or additions after
realization, embedded controls, radio tools, dropdowns, custom images or native
Cocoa customization are exposed.

`tool-enabled-p` and `tool-checked-p` read `(toolbar id)`; generic SETF writes
return the supplied Lisp value and are silent. Checked access requires a check
tool. Unknown IDs, separators and invalid receiver kinds reject. Same-toolbar
state writes and recursive activation defer during its action callbacks.

## Route actions

Toolbar events use `:menu`: wxEVT_TOOL is a native alias of wxEVT_MENU. Bind on
the toolbar or its frame, using existing ID/range filtering, user-data, immutable
snapshots and optional scoped contexts. Toolbar action origin IDs identify the
tracked toolbar; event IDs identify tools. Check clicks toggle native tool state
before callbacks; event checked/integer metadata copies that action state. Normal
tool activations carry false/zero checked metadata. Editing an event does not
write tool state. Default observers Skip and propagate, so avoid running the same
action twice in both toolbar and frame observers. A contextual toolbar handler
can consume it, or a single frame handler can handle shared action IDs.

`activate-toolbar-tool` exercises the native OnLeftClick command path after
realization: disabled tools return NIL without dispatch; enabled tools toggle
check state and return T for accepted activation, independent of handler/Skip
results. It does not simulate physical mouse/keyboard interaction. Callback
conditions are contained at the native boundary and re-signaled after dispatch.
Actual native and propagated toolbar-origin callbacks reject destruction/close
of their toolbar or ancestors, same-toolbar tool-state writes/activation and nested
pump/dispatch, modal or clipboard entry. Status text writes remain available.
Defer guarded actions until the callback returns. Unrelated window lifetimes
retain their existing contracts. Native :menu events remain independently
constructible/cloneable; such payloads do not activate or mutate toolbar tools.

## Status fields

`make-status-bar` accepts immutable `:fields` from 1 through 16 (default 1), with
native equal relative widths. `status-field-count` reads the count. Read/write
`status-text` with an optional zero-based field index (default 0):

```lisp
(setf (wxlambda:status-text status) "Ready λ"
      (wxlambda:status-text status 1) "Details on")
```

Empty and Unicode text round-trip; NUL, invalid indices and destroyed bars reject.
SETF returns the supplied string. Frame automatic menu/tool status-help updates
are disabled via SetStatusBarPane(-1), keeping fields application-owned; short
tooltips remain native. Width/style changes and push/pop status stacks defer.

## Backend and verification scope

The baseline is wxWidgets 3.2.12. macOS explicitly uses wxWidgets' ordinary
in-window toolbar controls. Native Cocoa NSToolbar construction can restore stale
raw tool-pointer placeholders when C++ toolbar addresses are reused; this path
is excluded. Construction temporarily sets `mac.toolbar.no-native` and restores
its previous effective string/HasOption value. wxSystemOptions has no public
remove operation: restoring an originally absent value as empty retains an
internal entry, so later environment changes to that option can be shadowed.
OSX builds without wxUSE_SYSTEM_OPTIONS reject toolbar construction. Other
backends retain wxWidgets' toolbar controls; hosted Windows/Linux GUI verification
is outstanding. Native Cocoa toolbar chrome is not claimed supported.

Run the shared-action example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/frame-actions.lisp --eval '(wxlambda-frame-actions:start)'
```

EXERCISE runs inside an application session and verifies shared menu/tool actions,
check synchronization, disabled activation and status updates. It uses in-memory
state. Automated tests cover both Lisps, native and propagated click callbacks,
Unicode fields, CLOS rollback, parent invalidation, repeated bar replacement and
backend option restoration. Physical tool clicks, keyboard/accessibility, display
scaling and hosted OS GUIs require separate manual verification.

See [menus](MENUS.md), [coverage](WXWIDGETS-COVERAGE.md) and
[Cadre installation](DOCSET.md).
