# Fixed splitter panes

`splitter-window` provides two persistent panel panes. Construct with
`make-splitter-window` or native-backed `make-instance`, create two distinct
`panel` children, lay out controls inside those panels, then call
`(split-window splitter first second)` outside RUN, pumps and callbacks.
Attachment is once only. `splitter-pane` indexes 0 and 1 and returns canonical
panel wrappers (NIL before attachment). The splitter owns attached panes;
destroy their splitter or ancestor instead of destroying individual panes.
Do not install a row/column sizer on the splitter itself.

`:orientation` is immutable: `:vertical` means left/right (default),
`:horizontal` means top/bottom. Generic SETF properties are
`splitter-sash-position` (default 0, native centering request),
`splitter-minimum-pane-size` (default 20, strictly positive), and
`splitter-sash-gravity` (default 0.5d0, real in [0,1]). Pixel arguments are bounded
by 536870911; sash positions are nonnegative. Native geometry is clamped and
may be deferred until layout. Getters report native state; exact requested
pixel geometry is not guaranteed. SETF returns the supplied Lisp value.
Positive minimum size and suppressed sash double-click keep both panes attached.

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
Single-pane Initialize, Unsplit, ReplaceWindow, dynamic orientation/styles,
resize/double-click/unsplit event bindings and complete native parity are deferred.
See [coverage](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).
