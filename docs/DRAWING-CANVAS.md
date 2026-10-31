# Retained drawing canvas and monitoring workflow

The monitoring example's `exercise` writes its default PNG in
`uiop:temporary-directory`, on every platform. Supply `:export-path` to choose
another destination; the bridge does not create missing parent directories.

ABI55 adds native-backed `drawing-canvas`, `make-drawing-canvas`,
`canvas-content`/SETF, `canvas-client-size` and `save-canvas-image`. This is a
bounded retained renderer for practical drawings and monitoring tools. Lisp
supplies copied scene data; native paint/resize handling owns the DC and renders
without calling Lisp. It does not expose paint callbacks, DC/bitmap/font handles,
native pointer adoption or C++ virtual methods to Lisp subclasses. Separately owned image/bitmap values and static displays are documented in [Images and displays](IMAGES-DISPLAYS.md).

```lisp
(defparameter view
  (wxlambda:make-drawing-canvas parent
    :content '(:background (245 248 252)
               :items ((:rect 20 20 120 40 (30 140 190))
                       (:line 20 80 200 80 (40 50 60) 2)
                       (:ellipse 180 20 32 32 (230 150 40))
                       (:text 20 100 "Signal λ" 16 (20 30 40))))))
(wxlambda:canvas-client-size view) ; fresh (width height) in DIP
(wxlambda:save-canvas-image view #p"/private/tmp/drawing.png" :width 320 :height 180)
```

`drawing-canvas` is a WINDOW subtype backed by an internal wxPanel subclass.
Required `:parent` is a WINDOW. `:content` defaults to white background and no
items. Direct MAKE-INSTANCE subclasses support slots/default initargs,
native-ready AFTER hooks and ordinary managed initialization rollback. The thin
generic factory validates `:class` and forwards initargs. Nest the canvas in a
row/column with `:proportion 1 :expand t`. Lisp ancestry is WINDOW, not PANEL.

## Scene format and bounds

Content is a proper four-element plist containing exactly `:background` and
`:items` (either key order accepted). RGB colours are three integer bytes0..255;
primitive colours are opaque. Items are drawn in supplied order with native clipping:

| Item | Data |
| --- | --- |
| `:rect` | `(:rect x y width height rgb)` filled rectangle with matching outline |
| `:ellipse` | `(:ellipse x y width height rgb)` filled ellipse with matching outline |
| `:line` | `(:line x1 y1 x2 y2 rgb stroke-width)` |
| `:text` | `(:text x y string point-size rgb)` single-line Unicode text in native Swiss/sans family |
| `:bitmap` | `(:bitmap x y width height pixels)` opaque row-major RGB byte vector |
| `:rgba-bitmap` | `(:rgba-bitmap x y width height pixels)` straight row-major RGBA byte vector, native alpha composition |

Coordinates/endpoints are signed integers within±10000. Rectangle/ellipse
extents are1..10000. Stroke width is1..32. Font point size is6..72; exact font
face, glyph coverage and metrics follow native fallback. Text rejects NUL/CR/LF.
At most512 items,65536 total UTF-8 text bytes and2MiB total bitmap bytes are
supported. Each bitmap dimension is1..256; PIXELS is a simple
`(unsigned-byte 8)` vector of exactly width*height*3 bytes for RGB or
width*height*4 bytes for RGBA. RGBA values use straight alpha; native display
conversion can premultiply and round colours. Screen bitmap extents use DIP,
while export extents use pixels. Scene bitmaps remain copied scene data; the
separate IMAGE/BITMAP resource API does not transfer pointers into this transport. All data crosses the ABI by value/copy, never pointer adoption.

Native scene compilation stages all items/strings/bitmaps before replacing the
old scene; invalid inputs preserve the old model. The Lisp writer returns its
supplied object. The stored normalized model, getter results, strings, colour
lists and pixel vectors are independent copies, so later input/output mutation
does not edit the scene. The getter validates native lifetime and reports the
last native paint failure before returning the copied model. A successful native
paint or new content clears that failure. Native callback conditions, if any,
are signaled after the committed Lisp/native model is synchronized.

Scene coordinates/extents/strokes are DIP on screen, converted through native
FromDIP. `canvas-client-size` reports native client dimensions through ToDIP;
integer conversions can round by a pixel. Native resize invalidates/repaints the
retained scene. It does not automatically rewrite the model. The monitor example
reads client size on each timer tick and rebuilds its bounded geometry. The
monitor geometry is clamped to 320..1200 by 180..700 scene units; smaller
viewports clip that drawing, and larger viewports can leave unused space. Native
window/font/content-scale behavior and antialiasing remain port-specific; this
is not a claim of pixel-identical multi-monitor rendering or globally bounded
inherited window geometry. There is no exposed resize/DPI event or zoom API.

## Main-thread and callback rules

Construction and all operations require the active GUI thread/session and valid
native lifetime. Content writes and PNG export reject Lisp event callbacks; use
an owner-bound deferred action or safe-idle UI timer body. Native staging,
refresh, rendering and export hold Input operation guards against source/ancestor
destroy/close, same-canvas recursion and nested GUI/modal/clipboard entry.
Native painting exceptions are contained and reported by subsequent canvas
reads; bitmap/image/font validity is checked before use. Native diagnostic logging
is suppressed inside these private operations so a failed export does not create
an unmanaged GUI error dialog.

```lisp
(wxlambda:defer-ui-action
  (lambda () (setf (wxlambda:canvas-content view) next-scene)) :owner view)
```

The renderer has no Lisp paint callback, so arbitrary callback code cannot retain
borrowed DC objects. Parent destruction disposes native scene bitmaps along with
the window. Getter/writer/export operations on stale wrappers reject normally.
See [CLOS API](CLOS-API.md) and [deferred actions](DEFERRED-UI-ACTIONS.md).

## PNG export

`save-canvas-image` takes a string/pathname and optional WIDTH/HEIGHT, default
640x480, each1..2048. It renders the same scene into a private 24-bit bitmap and
encodes PNG using an internal handler. Export dimensions and scene coordinates
are output pixels (one pixel per scene unit); clipping occurs at that viewport.
It does not scale the model to fit. Fonts retain native point-size metrics.
The encoder does not alter the global image-handler registry.

The supplied file is overwritten. Encoding finishes in memory before file open,
but file replacement is not atomic: write/flush/close failure can leave partial
output and does not restore previous contents. Errors signal NATIVE-ERROR.
Native path conversion is explicit UTF-8 with NUL rejection. Write, flush and
close status are checked, and the FILE is detached before checked close to avoid
pinned wxFFile retrying a consumed handle on a close error. There is no general
image/document/printing API or promise of arbitrary filesystem fault recovery.

## Demonstrated monitoring workflow

[Drawing monitor](../examples/drawing-monitor.lisp) combines native canvas,
Unicode text/font/colour/geometry, a copied RGB badge, an activity indicator,
owner-bound periodic UI updates and deferred Pause/Resume. Its data is explicitly
simulated; it does not run a background worker. EXERCISE self-stops after three
real timer ticks, verifies animation state, exports PNG and resumes before cleanup.
A separate native test decodes PNG and checks geometry/bitmap pixels, failed-scene
preservation, native resize/DIP roundtrip (one-pixel tolerance), synchronous
operation reentry, and owner teardown.

CMake requires IMAGE/LIBPNG/GRAPHICS_CONTEXT/ACTIVITYINDICATOR/STREAMS/FFILE plus
existing TIMER support; native components are core/base/adv. The rendering
implementation is modular but remains in the same checked bridge ABI. Source
inventory records these as bounded private value/retained-rendering adapters,
not standalone wxDC/image/font/colour class parity.

Physical window-paint fidelity, all font glyphs/locale/DPI combinations, hosted
Windows/Linux/Qt, accessibility and graphical Cadre remain unverified. Native
PNG rendering and local automated GUI tests are the declared acceptance target.
See [MVP acceptance](MVP-ACCEPTANCE.md), [Cadre installation](DOCSET.md) and
[native drawing reference](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/dc.h).
