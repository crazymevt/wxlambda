# Bounded scrolling panels for larger forms

ABI49 adds native-backed `scrolled-panel`, a WINDOW subclass using wxScrolledWindow
(wxScrolled<wxPanel>) for child controls. Required `:parent` is a window and
`:scroll-rate` defaults to `(0 16)`: horizontal disabled, vertical16 pixels per
unit. The exact two-integer list accepts0..256 per axis; zero disables that axis,
including `(0 0)`. Native baseline wxWidgets3.2.12; wxUSE_SCROLLBAR is required.
This is a control container, not a drawing canvas or a full wxScrolled binding.

```lisp
(let ((viewport (wxlambda:make-scrolled-panel parent :scroll-rate '(0 16))))
  (wxlambda:make-column viewport)
  (dotimes (index 24)
    (wxlambda:add-child viewport
      (wxlambda:make-text viewport :value (format nil "Field ~D" index))))
  (wxlambda:fit-scroll-content viewport)
  (setf (wxlambda:scroll-position viewport) '(0 10)))
```

`make-scrolled-panel` is a thin generic factory with validated `:class` and
forwarded subclass initargs. Direct subclasses support slots/default initargs,
native properties in AFTER hooks and managed failure rollback. Its Lisp base is
WINDOW, not concrete PANEL, preserving one native kind per construction. Notebook
pages and splitter panes requiring PANEL can contain a scrolled panel inside an
ordinary page/pane panel. Lisp subclassing does not implement C++ virtual hooks.
See [CLOS API](CLOS-API.md).

## Layout, extent and scroll units

Existing `make-column`, `make-row`, `add-child`, spacers and `layout-window` work
with the scrolling container. Children must have it as their direct parent.
Place the viewport in its outer row/column with `:proportion 1 :expand t` so it
receives available viewport space instead of requesting the whole document height.

`fit-scroll-content` requires an attached sizer and runs Layout/FitInside/Layout.
It refreshes the virtual document size from the sizer minimum and client viewport;
it does not resize the frame. Use it after content/layout changes. Layout alone
need not refresh extent immediately; native size events may also recalculate it.

`scroll-extent` returns a fresh `(width height)` list of current virtual pixels,
not scroll units, requested minimum, viewport size or scrollbar dimensions.
`scroll-rate` reads/writes a fresh/list pair of native pixels per unit. Rate
changes can change pixel origin and clamp positions; no preservation guarantee.
`scroll-position` reads native view-start `(x y)` in scroll units. SETF accepts
exactly two nonnegative integers up to1000000; native code clamps to its content/
viewport range and ignores disabled axes. It returns the supplied list, not the
accepted/clamped value; read back actual position. `-1` unchanged-axis sentinels
are not exposed. Lists are copied/not retained; there is no scalar/partial setter.

Native focus auto-scrolling, resize and platform scrollbar behavior can change
positions independently of these setters. Content extent is supported up to
1000000 pixels per axis. Explicit fit/rate/position operations check current
virtual/client dimensions and attached sizer minimum before native mutation;
oversized content rejects. Existing layout/native automatic resizing is not
globally policed by this new limit: applications must keep content within the
supported bound. No claim of arbitrary huge sizer arithmetic safety follows.
Rate/position argument bounds keep requested products below signed-int limits.

## Callback safety and native helper ownership

Scroll can synchronously repaint/update before moving children. Rate/position
writes and fitting reject during any event callback; defer them with an
owner-bound action. The native operation holds existing Input lifetime and
nested-entry guards so callbacks during synchronous native work cannot destroy/
close the viewport/ancestors or enter modal/clipboard/RUN/pump/safe-idle work.
Painting is allowed to run; no whole-operation event blocker is installed.
No new scroll event observer or explicit notification option is exposed.

```lisp
(wxlambda:on-click top-button
  (lambda ()
    (wxlambda:defer-ui-action
      (lambda () (setf (wxlambda:scroll-position viewport) '(0 0)))
      :owner viewport)))
```

wxWidgets owns a private pushed scroll-helper handler. Preserve it. Managed
`window-event-handler`, `push-event-handler`, `pop-event-handler`,
`remove-event-handler`, `process-window-event` and `process-window-event-locally`
reject on SCROLLED-PANEL rather than adopting/removing that helper. Ordinary
bindings and child command propagation still work. Native teardown owns helper
cleanup. There is no scroll-helper/native pointer adoption, drawing/DC preparation,
custom target window, coordinate conversion, focus policy override, physical
scroll event API or complete scrollbar/style parity in this slice.

## Example and verification

[examples/scrolling-panels.lisp](../examples/scrolling-panels.lisp) builds24 editable
fields in a bounded viewport with a deferred Back to top button. START is
interactive; EXERCISE verifies extent, actual scrolling and deferred return.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/scrolling-panels.lisp --eval '(wxlambda-scrolling-panels:start)'
sbcl --noinform --non-interactive --load ci/setup.lisp --load tests/scrolling-panels.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/scrolling-panels.lisp
```

Lisp checks cover native/subclass/factory construction, pair validation/copying,
clamp/disabled axes, fit without frame resizing, callback deferral, private-handler
API refusal, rollback and stale handles. Native probes test raw arguments and
oversized content. A test-only native ScrollWindow override injects a supported
callback inside actual bridge scrolling to check operation lifetime/reentry
protection independently of ordinary callback rejection. This is not physical
scrollbar or painting/accessibility validation. Runtime evidence is macOS3.2.12;
generic/GTK native scrolling and common window behavior were source-reviewed.
Hosted Linux/Windows/Qt GUI, physical wheel/trackpad/keyboard/focus navigation,
painting fidelity, DPI/resize combinations and graphical Cadre remain unverified.
Automatic tests do not read/replace system clipboard contents. See
[coverage](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).
