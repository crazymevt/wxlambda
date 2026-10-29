# Native bitmap toggle buttons

ABI62 exposes `wxBitmapToggleButton` through `bitmap-toggle-button`, a subtype of
`toggle-button`. Artwork is a native bitmap; it does not depend on font glyphs.
The existing text toggle remains available. The native build uses
`wxHAS_BITMAPTOGGLEBUTTON`; the pinned wxWidgets3.2.12 headers enable it on OSX,
MSW and GTK2/3 ports. Other ports can report unavailable rather than silently
substitute a label. Windows/Linux runtime tests have not run locally.

## Construction and state

```lisp
(wxlambda:bitmap-toggle-button-supported-p) ; active GUI/main-thread session
(wxlambda:make-bitmap-toggle-button parent bitmap :checked t)
(make-instance 'wxlambda:bitmap-toggle-button :parent parent :bitmap bitmap)
```

The factory accepts `:class` and forwarded subclass initargs. Required `:bitmap`
is a live session-owned `bitmap`; `:checked` defaults NIL. Bitmap controls use
native default ID/style/geometry, with an initially empty label. Direct CLOS
subclasses support slots, default initargs, native-ready AFTER hooks and managed
failure rollback. No raw native pointer/adoption is exposed. Unsupported-port
construction signals `native-error`. Checked state, generic label/window metadata,
tooltips and `:toggle-button-change` events retain the existing toggle protocols.

The control keeps a native copied value, so its source bitmap/image may be
explicitly destroyed after construction or assignment. Setters preserve checked
state. Toolkit theme, button dimensions, bitmap alpha composition, DPI resampling
and physical input remain port-native behavior.

```lisp
(setf (wxlambda:checked-p button :notify t) t)
(setf (wxlambda:bitmap-value button) replacement-bitmap)
(let ((copy (wxlambda:bitmap-value button)))
  (unwind-protect (wxlambda:bitmap-size copy) (wxlambda:destroy-bitmap copy)))
;; Equivalent owned readback:
(wxlambda:make-bitmap :source button)
```

Each readback returns a new session-owned `bitmap`, which the caller disposes.
Source and returned values do not retain the control or each other. Existing
image resource bounds apply: 1..4096 pixel axes, 256 image/bitmap values and
64Mi aggregate pixels. Normal bitmap only; alternate pressed/hover/disabled
artwork, bitmap bundles/scale policy, margins, arbitrary styles, grouping and full
class parity remain unbound.

## Callback and lifetime rules

Creation, bitmap copying/readback, bitmap writes/disposal require the active
main GUI thread and reject native event callbacks. Use owner-bound deferred UI
actions. Checked-state events, source/ancestor destruction guards, setter
reentry restrictions, automatic observer Skip and scoped context isolation are
inherited from the existing native toggle. Native parent destruction invalidates
controls; bitmap resources retain independent explicit/session lifetimes.

## Example and verification

[Bitmap toggle example](../examples/bitmap-toggles.lisp) provides START and a
bounded EXERCISE. `tests/bitmap-toggles.lisp` validates inherited state/events,
copy/dispose ownership, actual pixel readback after replacement, direct/factory
subclasses, rollback and callback refusal. The text-toggle and image-resource
regressions are also rerun. Local runtime verification uses macOS/SBCL and CCL;
GTK/MSW/Qt, physical mouse/keyboard, theme appearance and display/DPI transitions
remain unverified. See [text toggles](TOGGLE-BUTTONS.md) and
[image resources](IMAGES-DISPLAYS.md).
