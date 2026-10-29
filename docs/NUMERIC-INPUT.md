# Bounded sliders and integer spin controls

ABI45 adds native-backed `slider` and `spin-control` WINDOW subclasses. Both use
required `:parent` window, `:minimum` (default0), `:maximum` (100) and `:value` (0).
Endpoints must be integers within -1000000..1000000 with minimum strictly less
than maximum; the initial value must lie in that range. A custom positive range
therefore needs an explicit valid value. The bounds keep native arithmetic and
range spans within supported integer limits.

```lisp
(let* ((slider (wxlambda:make-slider panel :minimum -100 :maximum 100 :value 25))
       (spin (make-instance 'wxlambda:spin-control :parent panel
                                                :minimum -100 :maximum 100 :value 25)))
  (setf (wxlambda:numeric-value slider) 40
        (wxlambda:numeric-value spin :notify t) -20)
  (setf (wxlambda:numeric-range slider) '(0 10))) ; silently clamps to10
```

Slider `:orientation` is immutable `:horizontal` (default) or `:vertical`, queried
by `slider-orientation`. Spin controls use editable decimal integer text, unit
arrows and fixed wxSP_ARROW_KEYS, without wrap/base/increment/double variants.
Native style/tick/label/page/line/selection-range options are not exposed.
Use panels for ordinary layout. CMake requires wxUSE_SLIDER, wxUSE_SPINCTRL and
wxUSE_SPINBTN; the no-spin-button generic fallback is outside this slice.
Baseline: wxWidgets3.2.12.

`make-slider`/`make-spin-control` are thin generic factories with validated `:class`
and forwarded subclass initargs. Direct MAKE-INSTANCE supports slots/default
initargs and AFTER hooks with native properties ready. Abstract/ambiguous native
ancestry and invalid construction reject; managed initialization failure cleans
up native widgets and callback roots. Lisp subclasses do not imply native C++
virtual overrides. See [CLOS construction](CLOS-API.md).

## Value and range properties

`numeric-value` reads the native value. Generic SETF validates and writes a bounded
integer; out-of-range values reject without implicit clamping. Writes are silent
by default, including native numeric/text callback suppression. `:notify t` commits
the value then emits one native-class update event, even if the value is unchanged.
It returns the supplied integer; an escaping callback condition is contained and
re-signaled after the native call, without rolling the committed value back.

`numeric-range` returns a fresh two-element list `(minimum maximum)`. SETF accepts
exactly two valid endpoints, changes them silently and clamps the prior numeric
value to the new range. It returns the supplied list; mutating that list or a
getter result cannot change the native range. There are no separate bound setters
or range-change notifications. Constructor, range and silent value operations
suppress bridge numeric/text callbacks; the native controls retain their own
internal normalization.

A spin getter first validates GetTextValue using a decimal integer parse before
calling the native numeric getter. Empty/intermediate signs, non-integer strings,
NaN or overflow beyond that parser reject with NATIVE-ERROR; a range write also
needs a readable prior numeric value. An integer value write can recover an invalid
edit because it does not read the old edit first. For valid decimal text, native
current-versus-committed interpretation and focus-loss/arrow validation timing vary
by port. Do not treat this property as raw text or promise identical intermediate
edit behavior on all backends. There is no public string setter for spin text.

## Update events and callback boundary

Bind `:slider-change` (wxEVT_SLIDER, command family) and `:spin-change`
(wxEVT_SPINCTRL/wxSpinEvent, notify family) using BIND-EVENT, with existing IDs,
ranges, user-data, immutable snapshots and optional borrowed contexts.
`event-numeric-value` reads the integer payload from numeric snapshots/contexts;
unrelated snapshots return NIL and unrelated contexts reject. It is read-only.
The event payload is a snapshot of that event, not a live control property.
Existing raw EVENT-INTEGER context editing can change metadata without writing
widget state. Native spin veto is observational: it cannot portably undo a
committed numeric update. Use validated property writes/application policy.

These new kinds are native/explicit-notified-setter only: no synthetic, owned,
queued/posting constructor or detached clone support. Cloning an unrelated text
context retains its existing rules; numeric kinds reject even when the slider
uses an ordinary wxCommandEvent. Registered-kind numbering starts after both new
kinds and is protected by the ABI check.

Source numeric updates and spin-origin forwarded text callbacks protect the numeric
control and its ancestors against destruction/close, same-control value/range
writes, and modal/clipboard/nested dispatch entry. Composite child origins are
matched by identity against live numeric controls/children before any origin
pointer dereference. Creation/native-base teardown suppresses numeric/text delivery.
A spin can emit TEXT before continuing native processing and then SPINCTRL; raw
text callbacks therefore have the same lifetime guard. They are not commit events.

Silent writes to a different numeric control are allowed for slider/spin
synchronization; same-control recursive mutation rejects. Defer protected work
with [DEFER-UI-ACTION](DEFERRED-UI-ACTIONS.md), or apply it after callback return.
Default event observers Skip; contextual handlers choose propagation explicitly.
No callback ordering or hardware drag/spin timing promise is added.

## Example and verification scope

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/numeric-input.lisp --eval '(wxlambda-numeric-input:start)'
```

The example mirrors slider and spin updates using silent cross-control writes,
shows a value label, and narrows both ranges with deterministic clamping. EXERCISE
uses explicit notified setters to exercise the same callback logic; it does not
simulate hardware dragging/typing. Tests cover native-backed subclasses/defaults/
AFTER hooks, bounds, copy isolation, silent clamp, immutable/context payloads,
observational veto, same-control/lifetime gates, stale handles and rollback on
SBCL and CCL. Native probes directly dispatch authentic slider/spin/text event
classes outside setter guards, including propagation, raw invalid edit recovery,
constructor silence and native teardown with zero wx assertions.

Runtime evidence is macOS wxWidgets3.2.12. GTK/MSW native behavior was source-audited;
hosted Windows/Linux/Qt GUI, physical drag/spin/typing/accessibility, all transient
input-method/clipboard edit states and arbitrary allocation races remain unverified.
No system clipboard text is read/replaced in these automatic tests. This slice
does not bind wxSpinCtrlDouble, wxSpinButton, all scroll event families, raw numeric
native event construction or complete native control parity. See
[coverage](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).
