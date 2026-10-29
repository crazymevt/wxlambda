# General input events

ABI57 extends the existing `bind-event` protocol with raw keyboard, character,
mouse, wheel, focus, context-menu and size events. Payload readers accept an
immutable EVENT snapshot or the current innermost borrowed EVENT-CONTEXT.
They reject unrelated kinds; there are no payload SETF methods. Snapshots retain
copied numeric values, never native key objects or focus/window pointers.

| Kinds | Readers |
| --- | --- |
| `:key-down`, `:key-up`, `:char`, `:char-hook` | `event-key-code`, `event-unicode-key`, `event-raw-key-code`, `event-raw-key-flags`, `event-modifiers`, `event-auto-repeat-p`, `event-position` |
| `:left-down`, `:left-up`, `:left-double-click`; corresponding `:middle-*`, `:right-*`, `:aux1-*`, `:aux2-*` | `event-position`, `event-mouse-button`, `event-mouse-buttons`, `event-mouse-click-count`, `event-modifiers`, `event-dragging-p` |
| `:mouse-motion`, `:mouse-enter`, `:mouse-leave` | Same mouse readers; triggering button is NIL |
| `:mouse-wheel` | Mouse readers plus `event-wheel-rotation`, `event-wheel-delta`, `event-wheel-axis`, `event-wheel-lines`, `event-wheel-columns`, `event-wheel-page-scroll-p`, `event-wheel-inverted-p` |
| `:focus-gained`, `:focus-lost` | `event-related-window-id` |
| `:context-menu` | `event-position` |
| `:size` | `event-size` |

Raw key/mouse/focus/size events have `event-family` `:base` and are not command
events. Context-menu events are native command events. Existing generic metadata
and scoped Skip/propagation operations remain available with their normal native
meaning; those operations do not rewrite input payloads. No synthetic, owned or
cloned raw input events are supported.

## Values and coordinates

`event-key-code` is the native wx key code, not a Unicode character. Prefer
`event-unicode-key` for translated characters; it returns an integer Unicode
value or NIL for native WXK_NONE. It does not convert special key codes to
characters. Raw key code/flags are platform-specific integers or NIL when the
build lacks raw-key support. `event-auto-repeat-p` reports native availability;
not all ports distinguish repeat. KEY_UP does not pair one-for-one with repeated
KEY_DOWN. A key's position is generally the current mouse position, not a caret.

`event-modifiers` copies wx GetModifiers bits: ALT1, CONTROL2, SHIFT4, META8;
RAW_CONTROL is16 on macOS and aliases CONTROL on other ports. CONTROL/CMD
represents Command on macOS; RAW_CONTROL distinguishes its physical Control key.
Do not interpret raw OS key flags as this portable wx modifier bitfield.

`event-position` returns a fresh `(x y)` list. Key/mouse coordinates are native
client coordinates; context-menu coordinates are screen coordinates. A keyboard
context-menu invocation returns NIL for wxDefaultPosition. These are not an
automatic screen/client or DIP conversion. `event-size` returns a fresh
`(width height)` native whole-window extent, not client size or a DIP guarantee.

`event-mouse-button` is NIL or one of `:left`, `:middle`, `:right`, `:aux1`,
`:aux2`. `event-mouse-buttons` returns a fresh list of currently held buttons,
which differs from the triggering button. `event-mouse-click-count` is native
and can be -1 on unsupported ports. Dragging does not imply mouse capture;
capture/release/gesture APIs are outside this slice.

Wheel rotation is signed and may be smaller than its native delta; do not assume
one event equals one line. The axis is `:vertical` or `:horizontal`, with native
lines/columns-per-action and page-scroll metadata. Inverted reports native
natural-scroll metadata where supported. macOS already applies its configured
scroll direction; do not invert again merely because that flag is true.

`event-related-window-id` is the registered numeric bridge identity of the
previous/next focus window, or NIL for null/unregistered native windows. It does
not keep that window alive or promise it is still valid at delivery/read time.

## Observing versus consuming input

Default observer bindings automatically Skip, preserving native editing,
selection, accelerators, focus and sizer processing:

```lisp
(wxlambda:bind-event editor :key-down
  (lambda (event)
    (format t "key=~D unicode=~S modifiers=~D~%"
      (wxlambda:event-key-code event) (wxlambda:event-unicode-key event)
      (wxlambda:event-modifiers event))))
```

`:with-context t` bindings begin consumed, as for existing events. Explicitly
Skip unhandled input. Consuming KEY_DOWN prevents corresponding CHAR and can
suppress accelerators; consuming CHAR_HOOK usually prevents both KEY_DOWN and
CHAR. `allow-next-key-event` accepts only a live CHAR_HOOK context and calls
native DoAllowNextEvent: the hook can remain consumed while normal key events
are permitted. `event-next-key-event-allowed-p` reads copied/current hook state;
an earlier snapshot does not change when the live context is modified.

Focus and size handlers should normally Skip; native focus and sizer behavior
can break when they are consumed. Defer focus changes from FOCUS_LOST. Prefer
CONTEXT_MENU to assumptions about right-button ordering, which varies by port;
consuming right-button events can suppress native context-menu generation.

CHAR_HOOK propagates up the window hierarchy. Ordinary key/mouse/focus/size
events do not generally bubble like command events. A parent binding is not a
global child-input monitor. Context-menu follows command propagation. Existing
explicit native propagation operations do not promise every native port will
generate the same sequences.

## Lifetime, IME and verification scope

Readers validate kind and current context lifetime. Retaining a context after
its callback, or using an outer context during a nested callback, rejects.
Actual managed/composite origins and ancestors retain input callback guards:
destroy/close, modal/clipboard entry and nested GUI pumping defer. Creation and
early teardown suppress unpublished/retiring native input. Deferral is required
for focus/navigation changes and other callback-restricted operations.

Raw CHAR/key data is not a portable IME composition API. It does not expose
marked text, candidate lists or composition start/update/end/replacement ranges.
Let native text controls/STC handle composition and observe committed document
changes. STC IME presentation policy does not imply composition callbacks.
Physical IME/dead-key/emoji input remains a separate manual acceptance target.

[Input observer example](../examples/input-events.lisp) leaves native text editing
intact while displaying key, pointer and focus metadata. Its EXERCISE observes
native size delivery. Native fixtures use actual wx event classes to test copied
key/wheel/focus/context/size payloads, observer/context Skip, hook allow-next,
kind/context rejection, source/ancestor/native-entry guards and synthetic refusal.
They do not inject physical keyboard/mouse actions or prove OS input/IME parity.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/input-events.lisp \
  --eval '(wxlambda-input-events:start)'
```

See [event contexts](EVENT-CONTEXTS.md), [focus/navigation](FOCUS-NAVIGATION.md),
[deferred actions](DEFERRED-UI-ACTIONS.md) and [docset installation](DOCSET.md).


`event-cmd-down-p` and `event-raw-control-down-p` copy the native CmdDown and
RawControlDown states for key/mouse events. This preserves Command versus physical
Control distinctions on macOS without interpreting platform raw flags in Lisp.
On other ports the native methods define their relationship. They are distinct
from `event-command-p`, which identifies the wxEvent command-event class.
