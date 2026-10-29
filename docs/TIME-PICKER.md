# Time-of-day pickers for forms

ABI53 adds native-backed `time-picker`, generic `make-time-picker`, generic
`clock-time`/SETF and read-only `event-time`. Times are copied proper lists
`(hour minute second)`: hours 0..23 and minutes/seconds 0..59. NIL, incomplete or
longer lists, fractions, negative values, 24:00 and leap second 60 reject before
native mutation. Precision is whole seconds.

```lisp
(defparameter picker
  (wxlambda:make-time-picker parent :time '(9 30 0)))
(wxlambda:clock-time picker) ; fresh (9 30 0)
(setf (wxlambda:clock-time picker) '(23 59 59)) ; silent
(setf (wxlambda:clock-time picker :notify t) '(0 0 0)) ; midnight
```

The required `:parent` is a WINDOW; `:time` defaults to `(0 0 0)`, not the current
time. Native style is wxTP_DEFAULT. Editor/spin presentation, locale formatting,
12/24-hour display and accessibility follow the native port. The value adapter
always uses 24-hour tuple components, independent of display format. There is
no style, range, free editor text, nullable time, fractional second or scheduling
API in this slice.

`time-picker` inherits WINDOW. Direct MAKE-INSTANCE subclasses support slot
initargs, `:default-initargs`, native-ready AFTER hooks and managed failure/root
rollback. `make-time-picker` is a thin generic factory with validated `:class` and
forwarded initargs. Lisp subclassing does not add native C++ virtual methods.

```lisp
(defclass start-time (wxlambda:time-picker)
  ((record-id :initarg :record-id))
  (:default-initargs :time '(9 30 0)))
(make-instance 'start-time :parent parent :record-id 42)
```

## Wall-clock values without a date or timezone API

`clock-time` returns a fresh tuple of native committed time components. Input
lists are copied; SETF returns the supplied list. It does not parse intermediate
editor text for the app. Native getters with no supported valid committed time
signal NATIVE-ERROR. Native construction and writes verify component readback.

wxWidgets uses wxDateTime internally even for time-only controls. The bridge
constructs values on fixed local 2012-01-01, matching wxTimePickerCtrl::SetTime's
approach, so it does not use today's potentially discontinuous DST date. Native
construction must round-trip the requested time components or signals
NATIVE-ERROR. This is not a guarantee about every historical timezone rule.
Changing the process timezone during a GUI session is outside supported scope.
The native date portion and milliseconds are not exposed or interpreted as
application data. No timezone, offset, DST fold selection, UTC/Unix/Common Lisp
universal-time conversion or date-plus-time scheduling is implied. Applications
must separately decide how to combine a [calendar date](DATE-PICKER.md) and time
into an actual instant.

## Change snapshots and safe updates

Bind `:time-change` using `bind-event`. Programmatic writes are silent by default.
`:notify t` emits exactly one native wxDateEvent of wxEVT_TIME_CHANGED after
committed readback, even for an unchanged time. Physical editor/spin input retains
native event timing/count/order. Time-change is command family and cannot veto.

```lisp
(wxlambda:bind-event picker :time-change
  (lambda (event)
    (format t "Time components: ~S~%" (wxlambda:event-time event))))
```

`event-time` reads the captured native payload, independently of the current
picker value. It returns a fresh tuple, including `(0 0 0)` at midnight. Invalid
native payloads and non-time snapshots yield NIL. Snapshot values survive the
callback/window/session. A `:with-context t` borrowed context reads the current
native time payload and obeys current-innermost context isolation and expiry;
non-time contexts reject EVENT-TIME. The event date portion is ignored, including
dates outside the separate calendar-picker range. EVENT-DATE returns NIL for
time snapshots and rejects time contexts. EVENT-TIMESTAMP and raw command integer/
text metadata are not time-of-day encodings. Snapshot EVENT-TEXT is NIL.
There is no EVENT-TIME SETF, owned/synthetic time event or detached time clone.

Actual time origins, propagation and composite-child TEXT callbacks use the
shared Input guards. Same-picker writes and source/ancestor destroy/close reject;
nested GUI/RUN/pumps, modal/clipboard entry and safe-idle delivery are guarded.
Silent cross-picker synchronization is supported. Creation, early managed Destroy
and native-base teardown suppress relevant input callbacks; native Destroy
refusal/exception restores the guard if the object survives. Original callback
conditions re-signal after the native notifying setter returns without rolling
back committed time. Use owner-bound [deferred actions](DEFERRED-UI-ACTIONS.md)
for structural work from callbacks. The example's Reset start time button uses
that path.

## Build and verification scope

Link wxWidgets core/base/adv and require wxUSE_TIMEPICKCTRL=1 and wxUSE_DATETIME=1;
CMake rejects missing support. This reuses the existing date-picker component.
macOS packaging bundles transitive native dependencies. There is no native
wxDateTime resource exposed; existing date/time adapters remain bounded subsets.
See [CLOS API](CLOS-API.md), [time-picker.lisp](../examples/time-picker.lisp) and
[Cadre docset installation](DOCSET.md).

Deterministic tests cover midnight/last second, fixed-date construction, invalid
components, CLOS rollback, copied values/snapshots, silence/explicit notify,
native-origin/propagated/composite callbacks, context separation and teardown.
Physical editor/spin input, locale/AM-PM display, accessibility, historical
and DST timezone combinations, hosted Windows/Linux/Qt and graphical Cadre remain
unverified. Early composite teardown is an injected native probe on macOS,
not a hosted generic GUI claim. Tests never read or overwrite system clipboard
contents.

Native references: [time picker](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/timectrl.h),
[date/time events](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/dateevt.h).
