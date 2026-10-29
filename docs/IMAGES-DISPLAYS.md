# Images, bitmaps and displays

This slice binds session-owned native wxImage/wxBitmap values and wxStaticBitmap.
It supplies copying, pixel/channel access, PNG file decoding/encoding, native
scaling and display/coordinate queries. Image grids, zoom/pan policy, flame graph
models and image editing behavior belong in applications. Existing drawing-canvas
supplies bounded retained wxDC primitives; raw paint/DC lifetime adoption is not
exposed. Native input events can be bound on canvas/static-bitmap windows.

## Construction, ownership and pixel data

Use `(make-instance 'wxlambda:image :width 32 :height 32 :rgb bytes :alpha alpha)`
or MAKE-IMAGE with validated :CLASS/subclass initargs. RGB defaults to black;
ALPHA defaults to absent/opaque. Dimensions are1..4096 **pixels** on each axis.
RGB is a simple unsigned-byte8 vector of exactly width*height*3 interleaved bytes;
optional ALPHA is width*height opacity bytes (0 transparent,255 opaque).
The binding copies input, retains no Lisp pointers and returns independent vectors
from IMAGE-RGB/IMAGE-ALPHA. Mutating those vectors does not update native pixels.
Use generic SETF to replace channels. IMAGE-ALPHA always returns opacity bytes;
IMAGE-ALPHA-P distinguishes absent alpha. SETF ALPHA NIL clears the channel.
IMAGE-PIXEL reads/SETFs a four-element (R G B A) list with zero-based pixel X/Y;
writing nonopaque A creates alpha if absent. No premultiplication is used in
wxImage channels. Native mask transparency is normalized to alpha on import.

IMAGE-SIZE returns (width height). SCALE-IMAGE returns a new owned image using
native nearest-neighbour (:NEAREST) or high-quality (:HIGH, default) scaling;
MAKE-INSTANCE IMAGE :SOURCE image :WIDTH/:HEIGHT is the same construction path.
All image/bitmap resources together are bounded to256 values and64Mi pixels per
session; converted/display copies count too. Allocation failures signal NATIVE-ERROR.
Explicit DESTROY-IMAGE/DESTROY-BITMAP releases a resource; double disposal and stale
access reject. IMAGE-ALIVE-P/BITMAP-ALIVE-P query validity during an active session.
Session shutdown releases remaining resources. There is no garbage-collector
finalizer or raw pointer adoption. Each resource belongs to its original session.

Concrete IMAGE and BITMAP subclasses support slot initargs, :DEFAULT-INITARGS
and native properties in AFTER hooks. Mixed resource/window/event ancestry and
abstract resource construction reject before allocation. Failed managed initialization
rolls back nested native resources, windows and callback roots.

## Files and bitmaps

MAKE-IMAGE :PATH loads a local PNG. SAVE-IMAGE writes PNG to a supplied native
pathname and returns PATH. Decode/encode uses native wxPNGHandler without changing
global handlers. Input requires a PNG signature/IHDR,24 bytes..64MiB encoded data
and dimensions within the pixel cap before decoding. Unsupported formats, invalid
PNG, missing paths, decode/allocation or write/flush/close failures signal native
errors. Save may overwrite an existing path and a failure may leave partial output;
applications own atomic-save policy. Animated images, JPEG/GIF/SVG, metadata/color
profile editing and general image-handler registration are outside this slice.

MAKE-BITMAP :IMAGE creates an independently disposable native bitmap.
:SOURCE instead accepts BITMAP or STATIC-BITMAP to copy an existing native value. BITMAP-IMAGE
(or MAKE-IMAGE :SOURCE bitmap) converts to a new image. Native bitmap conversion
can premultiply/round alpha and discard RGB at fully transparent pixels; do not
use a display bitmap as a lossless pixel archive. Keep authoritative channel data in IMAGE; PNG decoding and scaling follow native wxImage semantics.
BITMAP-SIZE is the native backing **pixel** size. BITMAP-LOGICAL-SIZE,
BITMAP-DIP-SIZE and BITMAP-SCALE-FACTOR expose the separate native sizing concepts;
they are not interchangeable across ports.

STATIC-BITMAP is a WINDOW with :PARENT and :BITMAP construction, thin
MAKE-STATIC-BITMAP, subclass hooks and generic BITMAP-VALUE SETF. It retains its
own native bitmap value; disposing the source wrapper does not destroy the display.
BITMAP-VALUE reading returns a **new owned BITMAP every time**, to be explicitly
disposed. Native wxStaticBitmap may resample for high DPI: on local Cocoa a2x2
source reads back as4x4 backing pixels at scale2 with2x2 logical size. This is native
GetBitmap behavior, not identity with the originally supplied wrapper. Large DPI
readback copies can exceed the adapter pixel limit and reject. Bitmap conversion
and display copies obey native port color/alpha behavior.

## Displays and coordinates

DISPLAY-COUNT, DISPLAY-GEOMETRY (optional :CLIENT-P excluding taskbar/menu space),
DISPLAY-PPI and DISPLAY-SCALE-FACTOR query current native displays. Indices are
zero-based and transient across display changes. Geometry returns (x y width height)
in native screen units, with possibly negative origins. WINDOW-DISPLAY-INDEX returns
an index or NIL when native GetFromWindow finds none.

WINDOW-CLIENT-SIZE returns native client logical units. CLIENT-TO-SCREEN and
SCREEN-TO-CLIENT convert an (x y) pair; FROM-DIP/TO-DIP convert native wxPoint units
using the receiver. Each returns a two-element list. Input coordinates are bounded
to±1000000. WINDOW-CONTENT-SCALE-FACTOR is native GetContentScaleFactor. Cocoa logical
screen coordinates, physical backing pixels and DIP coincide differently on other
ports; convert explicitly before indexing image pixels. No physical display mode
changes, screenshot capture, screen pointer adoption or display objects are exposed.

All APIs require the active GUI/main thread, including detached image resources.
Reads of existing data/geometry are callback-safe. Writes, file IO, scaling,
construction, disposal and BITMAP-VALUE copying reject callback execution; defer
these operations with an owner-bound UI action. Native widget operations preserve
actual-origin/ancestor lifetime and native-entry guards. Worker delivery remains
QUEUE-THREAD-EVENT only. Native resources must be cleared before wx cleanup.

## Verification

Native and Lisp tests exercise copy isolation, RGB/alpha/pixel bounds, PNG round
trips, scaling, bitmap conversion and native high-DPI readback, static display
source disposal, coordinates and CLOS rollback. Local macOS evidence does not
certify other ports, physical monitor hotplug, color fidelity or physical input.
See [image example](../examples/image-viewer.lisp), [drawing](DRAWING-CANVAS.md)
and [input events](INPUT-EVENTS.md). Required native features: IMAGE/LIBPNG,
STREAMS/FFILE, STATBMP and DISPLAY. No third-party widgets are used.
