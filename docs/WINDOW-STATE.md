# Window visibility and input state

The first coverage slice completes the `wxWindow::Show(bool)`, `IsShown()`,
`Enable(bool)` and `IsEnabled()` operations for all currently bound windows.
Hiding through `show-window` with NIL also covers `Hide()`; disabling through
`enable-window` with NIL covers `Disable()`, preserving their changed-state
results as second values. It builds on the existing handle/lifecycle support; the rest of the window API
and C1 foundation inventory remain open.

## Showing and hiding

```lisp
(wxlambda:show-window window)       ; show, preserving the existing API
(wxlambda:show-window window nil)   ; hide
(wxlambda:shown-p window)           ; own shown flag
```

`show-window` returns the window as its primary value, so existing callers retain
their result. Its second value is true when wxWidgets changed the shown state
and false for an unchanged request. SHOW is a generalized boolean.

`shown-p` reads the window's own shown flag. A child can have this flag true
while a parent is hidden; this query does not promise it is visible on screen.
Frames initially have their shown flag false; child controls normally start true.
Use `layout-window` on the parent after hiding/showing a sizer child when the
layout should reclaim or restore its space.

## Enabling and disabling input

```lisp
(wxlambda:enable-window window nil) ; disable user input
(wxlambda:enable-window window)     ; enable it again
(wxlambda:enabled-p window)         ; effective state, including ancestors
```

`enable-window` returns the window and the native changed-state boolean as two
values. ENABLE is a generalized boolean. `enabled-p` includes disabled ancestors:
a child is effectively disabled while a parent is disabled. A child explicitly
disabled before disabling its parent stays disabled after the parent is enabled.

These functions require a live window in the active main-thread session.
Invalid or destroyed native handles signal `native-error`; false query results
and unchanged setters are ordinary results. Setters do not synthesize command
events. Hiding/disabling a control retains its handlers and ownership; explicitly
unbind handlers before destroying it under the existing lifecycle contract.

## Example and verification

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/window-state.lisp \
  --eval '(wxlambda-window-state:start)'
```

The example has separate buttons that toggle text visibility and input. Close
the frame to exit. Its `exercise` function queues the actual callbacks twice,
verifies both state transitions, unbinds handlers and destroys the window.
The focused test runs this example and checks setter return values, repeated
requests, ancestor semantics and destroyed/inactive handle errors:

```sh
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/window-state.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/window-state.lisp
```

Native ABI 8 introduces distinct boolean/error results (`0`/`1` versus `-1`).
Rebuild the native bridge with the Lisp changes. The old C `wxl_show` status
contract remains available; the Lisp wrapper uses `wxl_show_state` to expose
its second value. This does not implement `IsShownOnScreen`, intrinsic
`IsThisEnabled`, general event bindings or window subclassing.
