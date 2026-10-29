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
(wxlambda:make-bitmap-toggle-button parent bitmap :checked t :border :none)
(make-instance 'wxlambda:bitmap-toggle-button :parent parent :bitmap bitmap)
```

The factory accepts `:class` and forwarded subclass initargs. Required `:bitmap`
is a live session-owned `bitmap`; `:checked` defaults NIL. Bitmap controls use
native default ID/geometry, with an initially empty label. `:border :default`
keeps native styling; `:border :none` requests `wxBORDER_NONE` for flat controls.
Other border/style options reject. Selected/hover/focus appearance remains native. Direct CLOS
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

## Native palette and transparent artwork (ABI63)

`(window-colours window)` returns foreground and background as two fresh RGB
lists. It reads the native window palette, with current system-colour fallbacks
for invalid values; it does not force a light/dark theme or sample screen pixels.
Calls require a live window on the GUI thread; reads are callback-safe. Alpha,
colour resource wrappers and colour/theme setters are outside this query.

Use transparent image alpha and contrast-aware stroke colours for icon artwork,
then update the bitmap at a safe GUI boundary when the palette changes. An
owner-bound UI timer can observe colour changes even when document state is idle;
state/palette caching avoids unnecessary bitmap allocations. Theme/font rendering
policy remains application code, not a new automatic library theme manager.

The native invariant fixture checks the actual wxBORDER_NONE style bits, invalid
border rejection and colour query. Focused Lisp tests check the flat constructor,
query output and inherited lifecycle behavior. Pinned wxOSX3.2.12 maps the flag to
an unbordered native control; other ports honor their own wxWidgets rendering.
Physical theme switching and Windows/Linux appearance remain unverified.
