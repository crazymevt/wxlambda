# Keyboard accelerators for shared actions

ABI42 adds generic `frame-accelerators` and its SETF writer. The value is a proper
finite list of entries `(KEY ID &rest MODIFIERS)`. Construct a frame with the
`:accelerators` initarg, or replace the complete table while quiescent:

```lisp
(setf (wxlambda:frame-accelerators frame)
      '((#\n 3101 :ctrl) (#\d 3102 :ctrl) (:f1 3103)))
(setf (wxlambda:frame-accelerators frame) nil) ; clear explicit table
```

Keys are ASCII letters/digits or keywords `:f1` through `:f12`. Letters normalize
to uppercase. Modifiers are `:ctrl`, `:shift`, `:alt` and `:raw-ctrl`, each at most
once. `:ctrl` is wxWidgets' logical Control, corresponding to Command on macOS;
`:raw-ctrl` means physical Control there and aliases `:ctrl` on other ports.
Using both in one chord rejects. Letter/digit chords require `:ctrl`, `:alt` or
`:raw-ctrl`; shift alone or no modifiers would intercept ordinary text and are
outside this slice. Function keys may be unmodified.

Portable command IDs are integers 1..32767 (Windows native command storage/menu
lookup has a narrower bound than general menu/tool IDs). At most 256 entries are
supported. Duplicate effective native chords reject, including Control/raw-Control
aliases off macOS. Multiple different chords may target the same command ID.
Cyclic/improper lists, unsupported keys/modifiers and invalid IDs reject before
installation. Failed validation or native table construction leaves the previous
table intact. SETF returns the original supplied Lisp value.

Native tables copy entries and retain no wxMenuItem pointer or Lisp list. Getters
return fresh nested lists of normalized keys/IDs/modifiers (canonical modifier
order Control, Shift, Alt, raw-Control), and validate native frame liveness even
when the table is empty. Mutating input/output lists cannot change the installed
table. NIL installs wxNullAcceleratorTable. No table/entry resource wrappers,
string parsers, raw key codes, menu-item adoption or resource-file tables exist.

`:accelerators` defaults to NIL on `frame`. Native-backed subclass construction,
`:default-initargs`, slot initargs and AFTER hooks work; hooks can query the table.
Failed initialization cleans up the frame, table registry and callback roots.
Replacement/clear require an active main-thread application outside RUN, pumps,
callbacks and modal loops. Frame destruction invalidates the property.

## Shared actions and platform semantics

Bind contextual `:menu` handlers on the frame, filtered by command IDs, and route
them to the same Lisp action/model used by menu and toolbar handlers. Consume
handled actions (leave context Skip false) to avoid native fallback, which can
try a button command when the menu command is unhandled.

Accelerators dispatch native commands; they do not portably activate menu or
tool resources. On macOS/GTK the native table sends a direct menu command with
unset origin/default integer metadata, ignoring matching menu enable/check
state. Windows can resolve a matching menu item, honor disabled state and toggle
checks first. Origin/checked/integer snapshot metadata must not be treated as a
portable action-state protocol. Keep enable/check policy in the Lisp model and
synchronize views from that model. Distinct accelerator IDs mapped to the same
Lisp action are also supported. No automatic menu/tool state synchronization.

The example uses the same IDs and ignores native checked metadata: its model
owns toggles, gates disabled New actions and explicitly synchronizes menu/tool
views. Toolbar events consume before frame propagation; frame handlers consume
menu/accelerator commands. Tool check clicks already toggle their native state,
so same-toolbar callback state writes are avoided.

Explicit frame tables do not edit menu labels or own menu shortcuts. Plain
example menu labels avoid conflicting tab-delimited native shortcut hints.
Menu/control/system shortcuts, focus, keyboard layout, input methods and reserved
OS keys can affect delivery or precedence. Clearing this explicit table does
not remove menu-owned or system shortcuts. This is not a general key-event,
text-input, accelerator-text-parsing or global-hotkey API.

## Conservative callback boundary

Native accelerator commands lack a portable origin marker and can reach pushed
handlers or application handlers. Therefore, while any explicit frame table is
nonempty, **every managed `:menu` callback** conservatively protects all frames
with nonempty tables and their descendant windows. This includes unrelated menu
callbacks and synthetic menu events. Destruction/close of those windows rejects;
modal dialogs, clipboard and nested dispatch/pump entry also reject for the
callback duration. Property writes such as status feedback remain available.
Table changes already require quiescence. Defer destructive/modal/clipboard
work until after callback return (for example an application-owned idle action).
This slice does not add a deferred-action queue or claim modal Open/Save shortcuts
are complete. Clearing all explicit tables restores the previous menu callback
policy. Existing toolbar/navigation guards continue to apply independently.

## Example and verification

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/keyboard-actions.lisp --eval '(wxlambda-keyboard-actions:start)'
```

EXERCISE runs inside an active session, checks menu/tool/model synchronization,
disabled model policy and status feedback. Its synthetic commands verify action
logic, not key translation. Separate native macOS tests call the actual child
CHAR_HOOK dispatch path, verify frame-table lookup and pushed-handler propagation,
F12/Alt-digit commands and matching disabled/check-menu behavior. Tests also
cover atomic invalid replacement, native arrays/ranges, copied data, rollback,
clear, callback lifetime/loop gates and stale frames on SBCL and CCL.

wxWidgets 3.2.12 is the documented baseline; CMake requires wxUSE_ACCEL=1.
Physical keyboard input/focus, accessibility, alternate layouts/input methods,
reserved-key conflicts, hosted Windows/Linux translation and graphical Cadre
remain unverified. See [frame actions](FRAME-ACTIONS.md),
[coverage](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).
