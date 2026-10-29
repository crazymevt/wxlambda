# Required calendar dates in forms

ABI52 adds native-backed `date-picker`, generic `make-date-picker`, generic
`calendar-date`/SETF and read-only `event-date`. Dates are copied proper lists
`(year month day)`, with years 1900..2100, months 1..12 and valid Gregorian days.
1900 and 2100 are not leap years; 2000 is. NIL, incomplete lists, invalid days,
non-integers and years outside this range reject before native mutation.

```lisp
(defparameter picker
  (wxlambda:make-date-picker parent :date '(2028 2 29)))
(wxlambda:calendar-date picker) ; fresh (2028 2 29)
(setf (wxlambda:calendar-date picker) '(2030 12 31)) ; silent
(setf (wxlambda:calendar-date picker :notify t) '(2030 12 31))
```

The required `:parent` is a WINDOW; `:date` defaults to `(2000 1 1)` rather than
consulting today's clock. Native style is wxDP_DEFAULT|wxDP_SHOWCENTURY, with a
fixed inclusive range from 1900-01-01 through 2100-12-31. Construction verifies
native range and value readback before publication. The native editor, formatting,
calendar popup/spin presentation and century display depend on the port/locale.
There is no style selector, nullable date, free editor text, time picker, mutable
range or standalone wxDateTime object exposed by this slice.

`date-picker` inherits WINDOW. Direct MAKE-INSTANCE subclasses support slot
initargs, `:default-initargs`, native properties in AFTER hooks and managed rollback.
`make-date-picker` is a thin generic factory with validated `:class` and forwarded
initargs. Lisp subclassing does not add native C++ virtual hooks. Factory example:

```lisp
(defclass due-date (wxlambda:date-picker)
  ((record-id :initarg :record-id))
  (:default-initargs :date '(2028 2 29)))
(make-instance 'due-date :parent parent :record-id 42)
```

The adapter represents a local civil calendar date. It exposes no time-of-day,
Unix timestamp, Common Lisp universal-time, UTC conversion or timezone object.
Native wxDateTime construction must round-trip the requested local date; dates
not representable in the process's native local calendar signal NATIVE-ERROR.
This can matter for historical timezone changes that skipped an entire day.
Changing the process timezone during a GUI session is outside supported scope.
The native date getter must return a valid supported committed date or signals
NATIVE-ERROR. It does not parse or validate intermediate editor text for the app.
Input/output lists are independent copies; SETF returns the supplied list.

## Date-change events and safe updates

Bind `:date-change` via `bind-event`. Default programmatic writes are silent;
`:notify t` emits exactly one native wxDateEvent after native value readback,
even when the date is unchanged. Physical keyboard/calendar interaction has
native event timing/count/order; it is not constrained to that explicit-notify
contract. Date-change is a command family event and cannot be vetoed.

```lisp
(wxlambda:bind-event picker :date-change
  (lambda (event)
    (format t "Selected calendar date: ~S~%" (wxlambda:event-date event))))
```

`event-date` returns a fresh tuple from the captured event date. It freezes the
payload date, not a live read of the control. It remains readable after the
callback/window/session ends. For non-date snapshots or native date events with
invalid/out-of-scope dates it returns NIL. A `:with-context t` borrowed context
reads the current native date payload and rejects access after expiry or from an
outer context during a nested callback. Non-date contexts reject EVENT-DATE.
There is no EVENT-DATE SETF, synthetic/owned date event or detached date clone.
Raw command integer/text/selection metadata is not a date encoding and cannot
change the date payload; snapshot EVENT-TEXT is NIL for date-change.

The shared Input guards cover actual date origins, propagated callbacks and
composite-child TEXT events. Same-picker writes, source/ancestor destroy/close
and nested GUI/RUN/pumps, modal/clipboard entry and safe-idle delivery reject
during these callbacks. Silent cross-picker synchronization is supported. Native
creation and early generic composite Destroy/base teardown suppress input
callbacks. A refused native Destroy restores the guard state. Callback conditions
re-signal after the native setter returns without rolling back committed dates.

Use owner-bound deferred actions for navigation, disposal or structural work from
callbacks; see [deferred actions](DEFERRED-UI-ACTIONS.md). The example's Reset due
date button uses this path. See [date-picker.lisp](../examples/date-picker.lisp)
and [CLOS API](CLOS-API.md).

## Build and verification scope

Link wxWidgets core/base/adv and require wxUSE_DATEPICKCTRL=1 plus wxUSE_DATETIME=1.
CMake checks these requirements; the pinned installed headers export this control
from adv even though the interface documentation labels it core. macOS packaging
includes its transitive library dependencies. The copied adapter is not full
wxDateTime parity. See [Cadre docset installation](DOCSET.md).

Deterministic example/tests cover date bounds/leap years, copied values, native
range readback, constructor rollback, silence/explicit notify, native-origin and
propagated guards and teardown. Physical date editing/calendar/spin input, locale
formatting, accessibility, historical timezone combinations, hosted Windows/Linux/
Qt and graphical Cadre remain separate interactive verification. Generic teardown
is probed through an injected early Destroy phase on macOS; hosted generic GUI
behavior is not claimed. Tests do not read or overwrite system clipboard content.

Native references: [date picker](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/datectrl.h),
[date events](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/dateevt.h).
