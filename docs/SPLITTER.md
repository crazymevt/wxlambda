# Splitter panes

`splitter-window` manages one or two attached panel panes. Construct with
`make-splitter-window` or native-backed `make-instance`, create two distinct
`panel` children, lay out controls inside those panels, then call
`(split-window splitter first second)` outside RUN, pumps and callbacks.
`split-window` attaches the initial two panes to an empty splitter. `splitter-pane`
indexes 0 and 1 and returns canonical
panel wrappers (NIL before attachment). The splitter owns attached panes;
destroy their splitter or ancestor instead of destroying individual panes.
Do not install a row/column sizer on the splitter itself.

`:orientation` initially selects `:vertical` for left/right (default) or
`:horizontal` for top/bottom. ABI57 adds generic SETF `splitter-orientation` at a
safe structural boundary; it retiles current panes using native geometry.
Other generic SETF properties are
`splitter-sash-position` (default 0, native centering request),
`splitter-minimum-pane-size` (default 20, strictly positive), and
`splitter-sash-gravity` (default 0.5d0, real in [0,1]). Pixel arguments are bounded
by 536870911; sash positions are nonnegative. Native geometry is clamped and
may be deferred until layout. Getters report native state; exact requested
pixel geometry is not guaranteed. SETF returns the supplied Lisp value.
Positive minimum size and suppressed sash double-click keep both panes attached.
Explicit structural operations can change that attachment as described below.

## Explicit pane changes

```lisp
(wxlambda:initialize-splitter splitter first-panel) ; empty -> one pane
(wxlambda:resplit-window splitter second-panel)    ; one -> two panes
(wxlambda:replace-splitter-pane splitter 1 replacement) ; returns old pane
(wxlambda:unsplit-window splitter :remove 1)       ; returns removed pane
(setf (wxlambda:splitter-orientation splitter) :horizontal)
```

Initialize requires an empty splitter. Resplit requires exactly one pane and adds
its supplied panel as pane1. Replace addresses an existing pane0/1. Unsplit
requires two panes; removal0 leaves the former pane1 at index0. New panes must be
fully initialized distinct direct unsizered PANEL children of the splitter,
without existing attachment. No reparenting or foreign window adoption occurs.

Removed/replaced panes are hidden and remain live native children. They can be
reattached or explicitly destroyed; parent destruction invalidates them even
while hidden and detached. Replacement explicitly shows the new pane and hides
the old one. Canonical Lisp roots are published before native sizing/focus work
and updated after native success. Callback conditions can escape after a native
change has committed; these operations do not promise general rollback.

Initialize/Unsplit/Replace/Resplit and orientation SETF reject callbacks and
require quiescence or an outer owner-bound deferred UI action. Explicit Unsplit
does not synthesize or promise the sash-triggered UNSPLIT notification path.
Native geometry may clamp or change during later layout.

Bind native-only notify events `:splitter-sash-changing` and
`:splitter-sash-changed`. `event-sash-position` reads immutable snapshots or
current contexts; unrelated snapshots return NIL and unrelated contexts signal
NATIVE-ERROR. Its SETF writer accepts only a current CHANGING context, with
-1 to cancel or a bounded nonnegative position. CHANGING veto also cancels.
The event must be handled: leave SKIP false at the consuming handler; skipped,
unhandled native events can ignore edits/veto. CHANGED is observational:
veto cannot undo the committed position, and its sash writer is rejected.
Programmatic position writes are silent; resizing/gravity can emit CHANGED.
Snapshots and owned events have no writer; construction, synthetic delivery and
cloning of these event kinds are unsupported.

During a splitter operation or native/propagated sash callback, splitter-property
writes, destruction of its panes/splitter/ancestors, closing its ancestor frames, modal dialogs,
clipboard and nested dispatch/pumping are rejected. Defer these operations.
CLOS subclass slots/default initargs and AFTER hooks work; failed managed
initialization cleans up native children and callback roots.

Run the two-pane tree/list example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/splitter-manager.lisp --eval '(wxlambda-splitter-manager:start)'
```

Its EXERCISE function runs inside an active application session and verifies
model-driven tree/list navigation, canonical panes and sash updates. It is a
small in-memory manager, with no filesystem operations. The tree backend is
unavailable on wxQt pending safe asynchronous item identities. Physical sash
interaction/accessibility and hosted Windows/Linux remain unverified.

Supported native scope: wxWidgets 3.2.12 wxSplitterWindow and wxSplitterEvent.
Single-pane Initialize, explicit Unsplit/ReplaceWindow/resplit and orientation
SETF are implemented. Arbitrary styles, resize/double-click/unsplit event bindings
and complete native parity remain unbound. The dynamic workspace example in
`examples/aui-notebook.lisp` constructs panes through Initialize/Resplit; its
EXERCISE also removes/replaces/reattaches panes and changes orientation while
checking canonical identity. See [dynamic AUI workspaces](AUI-NOTEBOOK.md).
See [coverage](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).

## Safe deferred structural boundary (ABI43)

[Deferred UI actions](DEFERRED-UI-ACTIONS.md) run after native callbacks and
operations return. Their outer action body is an explicit safe structural
boundary even under RUN/full polling; the quiescent structural operations in
this guide can run there. Nested event callbacks retain their existing guards,
and nested RUN/pumps/finish remain forbidden. Optional owner/cancel/error rules
are documented in that guide.
