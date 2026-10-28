# Progress gauges

`gauge` is a native-backed CLOS `window` subclass for `wxGauge`, using the
wxWidgets 3.2.12 coverage baseline. Construct it directly or through the thin
`make-gauge` factory:

```lisp
(defclass progress (wxlambda:gauge)
  ((job :initarg :job :reader job))
  (:default-initargs :range 100 :value 0))

(defvar progress
  (wxlambda:make-gauge panel :class 'progress :job :download
                       :orientation :horizontal))
(setf (wxlambda:gauge-value progress) 25)
(setf (wxlambda:gauge-range progress) 200)
(wxlambda:gauge-value progress)       ; 25
(wxlambda:gauge-range progress)       ; 200
(wxlambda:gauge-vertical-p progress)  ; NIL
```

PARENT is required. RANGE defaults to 100 and must be a positive signed 32-bit
integer. VALUE defaults to zero and must be between zero and RANGE, inclusive.
ORIENTATION defaults to `:horizontal`; `:vertical` is also supported, with no
orientation writer. These restrictions are checked before passing invalid
values to wxWidgets. Lisp type/range checks signal `type-error`; a value above
the current range signals `native-error`. Invalid assignments preserve state.

The canonical writers are generic `(setf gauge-value)` and `(setf gauge-range)`;
both return the supplied integer. Reducing a range clamps existing progress to
the new maximum. Increasing it preserves progress. Range assignment calls
`SetRange` followed by `SetValue` to restore determinate mode consistently.
This explicit clamping policy is part of the Lisp binding contract.

For work with an unknown completion fraction:

```lisp
(wxlambda:pulse-gauge progress) ; returns PROGRESS
;; Call periodically while processing GUI events.
(setf (wxlambda:gauge-value progress) 50) ; resume determinate progress
```

`pulse-gauge` maps to `wxGauge::Pulse`; animation and progress readings while
pulsing are platform-dependent. Neither reading the value nor changing the range
after a pulse measures completed work. Assign an explicit value to resume
measured progress. Pulsing does not provide a timer or thread dispatch, and it
does not make blocking work responsive. Gauges emit no command events here.

The inherited layout, visibility, enabled state, logical name and destruction
operations work on gauges. Native ownership stays with the parent; destroyed
handles are invalid. All operations require an active main-thread GUI session.
Subclass slot initargs, defaults and AFTER hooks work as described in the
[CLOS guide](CLOS-API.md), including rollback when initialization fails.

This slice binds `wxGauge` construction, `GetValue`, `SetValue`, `GetRange`,
`SetRange`, `IsVertical` and `Pulse`. The constructor fixes ID to `wxID_ANY`,
position/size to native defaults and validator/name to native defaults; styles
are limited to horizontal/vertical. Two-step creation, custom IDs, additional
styles such as taskbar progress, and the remaining inherited API stay open.
Gauge support was introduced in native ABI 10. Registered event dispatch now requires
ABI 16; rebuild the bridge alongside the current Lisp bindings.

## Example and verification

The [gauge example](../examples/gauge.lisp) uses a subclass with a custom step
slot. It keeps completed work in a Lisp slot so the advance button resumes from
measured progress after a pulse, independent of native animation readings.
Buttons advance/restart progress and request indeterminate pulsing:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/gauge.lisp \
  --eval '(wxlambda-gauge:start)'
```

Its `exercise` runs those same callbacks, restores determinate mode and cleans
up handlers and windows. The focused test runs it and verifies orientations,
factory/subclass initargs, range clamping, invalid arguments, native type checks,
initialization rollback, inherited operations and stale/session handle errors:

```sh
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/gauge.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/gauge.lisp
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. The complete scenario
runner also includes `gauge`. See [docset installation](DOCSET.md) to install
the API, this guide and the example under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre.
