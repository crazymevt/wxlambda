# wxlambda

Common Lisp bindings for wxWidgets, using CFFI and a small C++ bridge with a
versioned C ABI. This is an application lifecycle prototype, not a complete
wxWidgets binding.

Implemented: application startup/shutdown, frames, panels, buttons, editable text,
static labels, two-state checkboxes, dropdown choices, row/column layouts, spacers, text/state access,
removable click/text/toggle/selection handlers, copied event payloads, and queued clicks.
Lisp objects wrap checked integer handles; native pointers never escape the bridge.

First-release lifecycle limits: **one GUI session per process**. Closing a window
does not release its registered Lisp handlers; unbind them before destruction,
or they remain rooted until session cleanup. Restart Lisp for a new GUI session.

## Build

Requirements: a C++17 compiler, CMake 3.16+, wxWidgets 3.2+, Common Lisp, ASDF,
and CFFI. Install CFFI using your preferred dependency manager, for example
`(ql:quickload :cffi)` with Quicklisp.

On macOS with Homebrew:

```sh
brew install cmake wxwidgets@3.2
cmake -S . -B build \
  -DwxWidgets_CONFIG_EXECUTABLE="$(brew --prefix wxwidgets@3.2)/bin/wx-config-3.2"
cmake --build build
```

On Linux with wxWidgets development packages and `wx-config` on PATH:

```sh
cmake -S . -B build
cmake --build build
```

Windows uses the same CMake project. Supply a wxWidgets installation compatible
with your compiler using CMake's FindwxWidgets settings. Multi-configuration builds
may put `wxlambda.dll` under `build/Release/`; load it explicitly before starting the
application with `(wxlambda:load-native-library #P"C:/path/to/wxlambda.dll")`.
Dependent wxWidgets DLLs must also be discoverable by the OS loader.

The bridge normally loads from `build/`, then falls back to the system library
search path. The Lisp/native ABI version (currently 7) is checked on load; rebuild
the bridge after updating the bindings. Installing the bridge with CMake does not
install the ASDF system.

## Run

From this directory, assuming Quicklisp is installed in `~/quicklisp`:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/hello.lisp
```

Or with Clozure CL:

```sh
ccl --load ~/quicklisp/setup.lisp --load examples/hello.lisp
```

Click the button to update the text; close the window to return to Lisp.
Run each example in a fresh process: this prototype supports one application
session per process.

For an existing image, load `wxlambda.asd`, then `(asdf:load-system "wxlambda")`.
The basic API is:

```lisp
(wxlambda:with-application ()
  (let* ((frame (wxlambda:make-frame :title "Hello"))
         (panel (wxlambda:make-panel frame))
         (text (wxlambda:make-text panel :value "Hello, λ"))
         (button (wxlambda:make-button panel :label "Click")))
    (wxlambda:make-column panel)
    (wxlambda:add-child panel text)
    (wxlambda:add-child panel button)
    (wxlambda:on-click button
      (lambda () (wxlambda:set-text text "Hello from Lisp!")))
    (wxlambda:show-window frame)
    (wxlambda:run)))
```

## Labels and checkboxes

`make-label` creates non-editable static text. `get-label` returns an independent
Lisp string and `set-label` updates it. These accessors take a label created by
`make-label`; raw strings use wxWidgets' label/mnemonic syntax. After changing a
label's preferred size, call `layout-window` on its parent to reflow the layout.

`make-checkbox` creates a two-state checkbox, unchecked by default. `checked-p`
returns `t` or `nil`. The `:checked` constructor keyword and `set-checked` accept
generalized booleans: `nil` means unchecked, any other value means checked.
`set-checked` returns the requested state normalized to `t` or `nil`; a handler
may change the live state again before the setter returns.

```lisp
(let ((caption (wxlambda:make-label panel :label "Preferences"))
      (box (wxlambda:make-checkbox panel :label "Include a greeting" :checked t)))
  (wxlambda:add-child panel caption)
  (wxlambda:add-child panel box :expand nil)
  (wxlambda:on-toggle box
    (lambda (event)
      (wxlambda:set-label caption
        (if (wxlambda:event-checked-p event) "Greeting enabled" "Greeting disabled"))
      (wxlambda:layout-window panel)))
  (wxlambda:set-checked box nil)            ; silent
  (wxlambda:set-checked box t :notify t)    ; synchronous toggle event
  (wxlambda:off-toggle box))
```

`on-toggle` takes a function accepting one event, with kind `:toggle`, the original
checkbox as its source, and a boolean `event-checked-p` snapshot. Native user
interaction emits events. `set-checked` is silent by default; `:notify t` emits one
synchronous event, including when the value is unchanged. The snapshot survives
nested changes to the control and later window destruction. Replacing a handler
releases its registered closure; `off-toggle` releases it and is idempotent for a
live checkbox, including from inside the handler. Setter errors follow the same
recoverable boundary as text-change handlers.

Run [examples/form.lisp](examples/form.lisp) for a form using labels, text input,
a checkbox, a greeting-style dropdown, row/column layouts, and submit/reopen
buttons:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/form.lisp
```

The reusable application lives in `examples/form-app.lisp`. For live development,
load that file and evaluate `(wxlambda-form:start-live)` from a terminal REPL:

```lisp
(load "examples/form-app.lisp")
(wxlambda-form:start-live)
;; Evaluate these inside the application REPL:
(wxlambda:set-text wxlambda-form:*name* "Ada λ" :notify t)
(wxlambda:set-selection wxlambda-form:*style* 1 :notify t)
(wxlambda:queue-click wxlambda-form:*submit*)
(wxlambda-form:set-submit-action
  (lambda () (wxlambda:set-label wxlambda-form:*status* "Custom submit")))
(wxlambda:queue-click wxlambda-form:*submit*)
(wxlambda-form:set-submit-action) ; restore the named SUBMIT-FORM action
(wxlambda-form:close-window)      ; unbind every handler before destruction
(wxlambda-form:open-window)       ; recreate with retained input values
(wxlambda:stop)
```

The “Reopen form” button performs the same explicit unbinding and recreation
within a GUI callback. The default submit action resolves `submit-form` by name,
so redefining that function also takes effect live. The application uses public
bindings throughout; no new native operations were needed. It retains the
one-session-per-process limit. If the user closes the native frame before
explicit unbinding, its stale handles cannot release the registered closures;
those remain rooted until session cleanup. Normal startup exits with the last
window; the application REPL remains available after closing it.

## Layout

Use `make-column` for vertical layout and `make-row` for horizontal layout. A
window has one sizer. To nest orientations, create a child panel, give it its own
row or column, then add that panel to its parent's layout. `add-child` requires
the child's native parent to be the layout's window.

```lisp
(wxlambda:make-row panel)
(wxlambda:add-child panel text :proportion 1 :border 0)
(wxlambda:add-spacer panel 12)
(wxlambda:add-child panel button :border 0 :expand nil :align :center)
```

`add-child` accepts these layout options:

| Keyword | Default | Meaning |
| --- | --- | --- |
| `:proportion` | `0` | Nonnegative weight along the layout axis; zero keeps the item's preferred extent |
| `:border` | `8` | Nonnegative spacing on each of the four sides |
| `:expand` | `t` | Fill the available space across the layout axis |
| `:align` | `:start` | With `:expand nil`, place at `:start`, `:center`, or `:end` across the axis |

In a row, alignment controls vertical placement; in a column, it controls
horizontal placement. `:expand t` with `:align :center` or `:end` is rejected.
Proportions and stretch allocation remain subject to native minimum sizes.
The defaults preserve the original `add-child` behavior.

`(add-spacer panel size)` inserts fixed space along the layout axis.
`(add-stretch panel :proportion 1)` inserts flexible space with a positive weight,
sharing available room with other proportional items. Both return the panel.
Invalid sizes, proportions, parent relationships, and duplicate insertions signal
errors before the sizer is modified.

`layout-window` recomputes a window's layout. `add-child`, `add-spacer`, and
`add-stretch` also lay out their parent immediately. Layouts participate in
wxWidgets' normal resize handling. `window-position` returns X and Y as two values,
relative to the parent's client area (screen coordinates for top-level frames).
`window-size` returns outer width and height as two values. Dimensions and spacing
use wxWidgets window coordinates, not framebuffer pixel counts; native minimum
sizes and display scaling remain wxWidgets' responsibility.

Run [examples/layout.lisp](examples/layout.lisp) to try nested rows and columns,
a text field that takes the available width, and a footer with flexible spacing:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/layout.lisp
```

## Events

Existing `(on-click button (lambda () ...))` handlers still take no arguments.
Opt into an event argument with `:with-event t`:

```lisp
(wxlambda:on-click button
  (lambda (event)
    (format t "Clicked ~S~%" (wxlambda:event-source event)))
  :with-event t)

(wxlambda:on-text-change text
  (lambda (event)
    (format t "New text: ~A~%" (wxlambda:event-text event))))

(wxlambda:set-text text "Silent update")
(wxlambda:set-text text "Notify the handler" :notify t)
(wxlambda:off-text-change text)
```

`event-p` recognizes event snapshots. Their read-only slots are exposed through:

| Accessor | Click event | Text-change event | Toggle event |
| --- | --- | --- | --- |
| `event-kind` | `:click` | `:text-change` | `:toggle` |
| `event-source` | Original button | Original text control | Original checkbox |
| `event-text` | `nil` | Copied string, including `""` for empty text | `nil` |
| `event-checked-p` | `nil` | `nil` | Copied `t` or `nil` state |

Text-change handlers always take one event argument. Typing into the native text
field emits events. `set-text` stays silent by default to avoid update loops;
`:notify t` uses wxWidgets' notifying setter and may call the handler immediately,
before the setter returns. Handlers can therefore run before `run` starts.

The text payload describes that event, rather than a later read of the control.
It remains valid across nested updates, handler unbinding, and window destruction.
Event text is an independent Lisp string; callers can retain or modify their copy.
The source wrapper remains available, but native operations on it fail after its
window is destroyed. Event slots do not expose any borrowed native pointers.

Use `off-click`, `off-text-change`, or `off-toggle` to release a handler. Registering again
replaces and releases the previous closure. Both removals are idempotent for a
live control of the correct type and may be called inside the handler itself.

## Dropdown choices

```lisp
(let ((choice (wxlambda:make-choice panel
                :items '("Small" "Medium" "Large λ") :selection 1)))
  (wxlambda:on-selection choice
    (lambda (event)
      (format t "Selected index: ~S~%" (wxlambda:event-selection event))))
  (wxlambda:set-selection choice 2)           ; silent
  (wxlambda:set-selection choice 0 :notify t) ; synchronous notification
  (wxlambda:selection choice))               ; => 0
```

`make-choice` copies a list of strings in the supplied order. Items are fixed
for the life of the control. Selection indices are zero-based; `nil` means no
selection and is the default, including for a nonempty list. Empty lists are
supported. An out-of-range index signals an error without changing the selection.
The dropdown participates in layouts and parent ownership like other controls.

User selection and `set-selection` with `:notify t` deliver one `:selection`
event. `event-selection` is an immutable index snapshot (or `nil`), independent
of later selection changes. Other event kinds have a `nil` selection slot.
`set-selection` returns the requested index even if a handler changes it again;
explicit notification also fires for an unchanged index. Handler errors are
re-signaled at the setter boundary. `on-selection` replaces the previous handler;
`off-selection` releases it and is idempotent for a live choice, including inside
its callback. As with other strings, embedded NUL characters are unsupported.

## Lifecycle contract

- `with-application` initializes the toolkit and cleans up on normal return or a
  Lisp error. Call `run` explicitly to enter the blocking GUI loop.
- GUI calls must run on the main thread. On macOS, the SBCL command-line main
  thread works directly. CCL uses a small implementation adapter to execute the
  application body on its initial process, synchronously. Caller thread dynamic
  bindings are not transferred by that adapter; establish needed bindings inside
  the application body. Starting from an SBCL worker/SLIME thread is rejected.
- Native GUI code assumes masked floating-point traps. The SBCL adapter masks
  them for the application session and restores the caller's state afterward.
- Parents own their children. Closing/destroying a parent invalidates child
  handles. Lisp wrapper objects may survive, but subsequent operations signal
  `native-error`. Destruction can be deferred by wxWidgets until event processing.
- Click handlers take zero arguments by default, or one event with
  `:with-event t`. Text-change and toggle handlers take one event. Handler replacement or
  explicit unbinding releases the registered closure. Queued clicks use the
  handler present when they are dispatched, so unbinding suppresses queued clicks
  and rebinding redirects them to the new handler. Closures for destroyed controls
  remain rooted until session cleanup unless explicitly unbound before
  destruction. There are no GC-driven native destructors.
- Errors in synchronous text-change or toggle callbacks are saved until the notifying
  setter returns to Lisp, then re-signaled so its caller can handle them without
  stopping the loop. Errors escaping the outer event callback request a loop stop
  and are re-signaled after the event loop returns. This also covers notifying
  setters called before the event loop starts. Errors escaping the application
  body trigger cleanup. Do not use `throw`, `return-from`, or other nonlocal exits
  across a callback's foreign boundary. Handle expected conditions inside the
  handler or around the notifying setter.
- `get-text` returns an independent Lisp string containing the current text;
  `set-text` updates it. Both use UTF-8. The native `wxl_get_text` buffer is borrowed
  until the next getter call on that thread and is copied immediately by Lisp.
  A null pointer reports failure; an empty string is a successful empty value.
  Strings containing embedded NUL characters are not supported by this interface.
- One GUI session per process; no application restart, background-thread GUI
  scheduling, or virtual-method overrides yet.

## REPL development

From a main-thread terminal Lisp REPL, initialize an application and enter its
cooperative REPL (do not call `run` in the setup body):

```lisp
(defvar *frame*)
(defvar *text*)
(wxlambda:with-repl-application ()
  (setf *frame* (wxlambda:make-frame :title "Live development")
        *text* (wxlambda:make-text *frame* :value "Edit me"))
  (wxlambda:show-window *frame*))
```

Then evaluate `(wxlambda:set-text *text* "Updated live")`, redefine functions
used by callbacks, or replace handlers without closing the application. Evaluate
`(wxlambda:stop)` or send EOF to leave and clean up. Closing the last window
keeps this REPL available so you can create another frame in the same session.
`run` signals an error inside this REPL because events are already being processed.
Errors enter the host Lisp debugger by default (`:on-error :debug`). Evaluation
conditions reach it before unwinding, preserving their local restarts. Choose
`wxlambda:return-to-application-repl` to abandon the current operation and keep
the session, or use an available evaluation restart to resume the form. Debugger
I/O uses the host's `*debug-io*`; `:input` and `:output` configure only application
REPL I/O. Setup-body errors propagate to the caller and clean up the session.

For scripts or a host with debugging disabled, use
`(wxlambda:with-repl-application (:on-error :report) ...)`. This prints the
condition type/message and up to 20 backtrace frames on SBCL/CCL before abandoning
the operation and continuing. Other Lisps report that a backtrace is unavailable;
they are not validated platforms. The debugger policy respects the host's
debugger hooks and batch-mode restrictions; it does not override them.

Callback errors are delivered to this policy only after returning safely from
the native callback. Their original condition is preserved, but callback-local
restarts and callback stack frames have already unwound. Debugger backtraces for
these errors show their Lisp delivery boundary. This small REPL prints all
returned values; it does not provide the host REPL's history variables or editor
integration. Optional `:input` and `:output` streams default to standard I/O.

Events are pumped while waiting for input. Incomplete forms, long evaluations,
and debugger stops pause GUI processing. `process-events` is rejected while the
application REPL's debugger is active; queued events resume after recovery.
During long evaluations outside the debugger, call `process-events` explicitly
when useful. The main-thread requirements above still
apply. Editor REPLs such as SLIME and SLY are unsupported by this helper; use
a terminal Lisp REPL. In particular, SBCL worker-thread startup is rejected. After leaving,
start a fresh Lisp process for another session.

For a complete live-development example, load `examples/live.lisp` and call
`(wxlambda-live:start)` from a terminal REPL. While it runs, try:

```lisp
(wxlambda:set-text wxlambda-live:*text* "Changed live")
(defun wxlambda-live:clicked ()
  (wxlambda:set-text wxlambda-live:*text* "New callback implementation"))
(wxlambda:queue-click wxlambda-live:*button*)
(wxlambda-live:close-window) ; explicitly unbinds before destroying the frame
(wxlambda-live:open-window)  ; another frame, within the same session
(wxlambda:stop)
```

The registered callback resolves `clicked` on each invocation, so redefining it
takes effect without replacing the handler. `close-window` releases the handler
before destroying its controls. If the user has already closed the native
window, its handles are stale and that handler remains rooted until session
cleanup. Prefer `close-window` during repeated development cycles.

Editor integration is explicitly deferred for the first release. Local Swank
2.32 wire probes on Apple Silicon macOS found:

| Probe | Observed execution |
| --- | --- |
| SBCL 2.6.9, `nil` / `:fd-handler` styles | Evaluation, application body, and callback on the launch thread |
| SBCL 2.6.9, `:spawn` control | Worker GUI startup rejected by the native main-thread check |
| CCL 1.13, `:spawn` | Caller on worker; application body and callback on initial process; caller dynamic binding absent |

These probes execute one RPC containing a complete GUI session. They do not
demonstrate an editor workflow with a persistent session, GUI responsiveness
between requests, evaluation interruption, or debugger transport. The terminal
application REPL is the first-release workflow. No editor adapter or background
scheduler is selected; their ordering, values, conditions, shutdown, and
reentrancy contracts must be established before future implementation.

To reproduce these optional experiments with an existing Quicklisp installation
containing Swank, run:

```sh
python3 tests/editor-probe.py nil sbcl --noinform --no-sysinit --no-userinit --non-interactive --load ~/quicklisp/setup.lisp
python3 tests/editor-probe.py :fd-handler sbcl --noinform --no-sysinit --no-userinit --non-interactive --load ~/quicklisp/setup.lisp
python3 tests/editor-probe.py :spawn sbcl --noinform --no-sysinit --no-userinit --non-interactive --load ~/quicklisp/setup.lisp
python3 tests/editor-probe.py :spawn ccl --no-init --batch --load ~/quicklisp/setup.lisp
```

These are optional local experiments using Swank internals and loopback sockets,
separate from pinned CI dependencies. `:spawn` probes use a persistent listener
to keep Swank's accept loop off the launching thread. Their process and listener
are shut down after the request. They are not supported editor configurations.

The bridge catches native exceptions at exported operation boundaries and exposes
error text through `wxl_last_error`. Its text is borrowed until the next bridge
operation on the same thread. See `native/wxlambda.h` for the C interface.

## Verification

These tests briefly open real windows, queue a button event, then exit. They need
an interactive graphical session (or a suitable display server on Linux).
The 23 scenarios each run in a fresh process with a 30-second timeout:

```sh
python3 tests/run.py
python3 tests/run.py ccl --no-init --batch --load ~/quicklisp/setup.lisp
```

A separate preflight loads the native bridge, checks its ABI, and prints actual
Lisp/OS/architecture and CFFI source metadata before any GUI scenario. To retain
evidence, set `WXLAMBDA_TEST_REPORT` to a new directory before running the suite:

```sh
WXLAMBDA_TEST_REPORT=test-results python3 tests/run.py sbcl --noinform \
  --no-sysinit --no-userinit --non-interactive --load ci/setup.lisp
```

The runner records per-process logs and `summary.json` with the source commit,
working-tree status, command, platform, scenario results, durations, and exit
codes. It preserves partial timeout output, stops at the first failure, and
returns a failing exit code. Preflight is separate from the 23 GUI scenarios;
a load failure cannot count as GUI coverage. An existing report directory is
rejected to avoid mixing old and new evidence. Reporter checks can be run with
`python3 -m unittest discover -s tests -p test_runner.py`.

Retained local Phase 3 reports live in
`evidence/platform/{sbcl,ccl}-20261028-phase3/`. The ignored `evidence/` directory
is separate from the disposable `.ci-deps/` cache; keep evidence there when
retaining local runs across dependency-cache cleanup.
The newer 23-scenario Phase 4 runs are retained in
`evidence/phase4/{sbcl,ccl}-20261028/`; the Phase 3 records retain their original
21-scenario count.

Choice tests cover selection bounds, empty/Unicode item lists, nested selection
snapshots, handler replacement/removal, callback errors, and stale handles.

Coverage: callback dispatch and replacement, empty/long/Unicode text round trips,
independent text copies, explicit closure release, queued click suppression,
rebinding and self-unbinding, parent/child invalidation, wrong-type handles,
duplicate sizer insertion, cleanup on setup and callback errors, and rejection
of a second session. Event tests cover source/kind metadata, copied Unicode and
empty text payloads, nested updates, silent versus notifying setters, self-unbinding,
and text-handler errors before and during the loop, including caught errors that
must leave the loop running. Layout tests verify actual child geometry for rows,
columns, proportional allocation, fixed/flexible spacing, borders, and alignment,
plus invalid options and stale handles. Control tests cover label text copies,
checked-state access, nested toggle snapshots, handler replacement/unbinding,
and caught/uncaught toggle errors. A timed Gray input stream tests delayed
whitespace/CRLF, GUI callback dispatch while input is unavailable, separate
evaluation/callback error recovery, and delayed EOF cleanup on SBCL and CCL.
This simulates delayed terminal input; it does not validate an actual terminal
or editor transport.
Debugger tests verify original condition identity, live evaluation restarts,
abandon-form recovery, safe delivery of callback errors, and paused GUI dispatch.
They install automated debugger hooks on the GUI thread. A separate SBCL
pseudo-terminal check entered the debugger, selected the application REPL
restart, evaluated another form, and stopped cleanly. CCL terminal debugger I/O
has not been verified.
Unicode rendering, manual mouse/keyboard input,
and user-generated toggle events have not been visually verified; automated text
and toggle events use notifying setters.

The live-development example test exercises text updates, callback function
redefinition, 50 explicit bind/unbind cycles with registry-release checks,
unbinding before destruction, stale handles, and frame recreation in one session.
Two representative-form scenarios exercise ordinary blocking startup and the
application REPL, including Unicode input, checkbox/dropdown state, queued
handler replacement, named action redefinition, unbinding all event kinds, and
state-preserving window recreation from Lisp and a GUI callback. These use
queued/notifying events; manual input and visual layout have not been checked.

Locally tested and intended combinations are distinct:

| Platform | Lisp | Validation |
| --- | --- | --- |
| Apple Silicon macOS, wxWidgets 3.2.12 | SBCL 2.6.9, CCL 1.13 | Local preflight and 23-scenario GUI suites; dependency cache switching verified separately |
| Linux | SBCL | Intended; hosted job configured, no hosted results recorded |
| macOS hosted runner | SBCL | Intended; hosted job configured, no hosted results recorded |
| Windows x64 | SBCL 2.6.9 intended, not validated | Native build, preflight, and GUI suite configured; no runtime results |

## Continuous integration

`.github/workflows/ci.yml` runs on pushes, pull requests, and manual dispatch.
The configured jobs are:

| Runner | Native build | Lisp GUI tests |
| --- | --- | --- |
| Ubuntu 24.04 | Distribution wxWidgets 3.2, GCC | SBCL, 23 scenarios under Xvfb |
| macOS 15 | Homebrew wxWidgets 3.2, Clang | SBCL, 23 scenarios |
| Windows Server 2022 x64 | wxWidgets 3.2.12 from source, MSVC | Pinned SBCL 2.6.9, preflight and 23 scenarios; unexecuted |

These are configured checks, not a claim that hosted jobs have passed. No hosted
results have been recorded. This checkout currently has no Git remote, so a
GitHub repository association is needed to dispatch or inspect the relevant
runs. CCL remains locally tested on macOS, outside this CI matrix.

All jobs run reporting checks, native loading/ABI preflight, and the GUI suite
(including Unicode copies, callbacks, controls, cleanup, and REPL recovery).
An `always()` upload step retains generated `test-results/` logs and JSON reports
for 30 days, even after test failures. A job that fails before tests start may
have no test report; inspect its Actions logs as well as its conclusion. Hosted
Windows, Linux, and macOS execution remains required before claiming validation.

The Windows build follows the upstream [MSVC makefile instructions](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/docs/msw/install.md).
It builds wxWidgets release DLLs from a pinned commit, then passes explicit
`wxWidgets_ROOT_DIR`, `wxWidgets_LIB_DIR`, and `wxWidgets_CONFIGURATION=mswu`
settings to CMake. System package versions on Linux/macOS follow their package
repositories; these native dependencies are not locked to an exact patch release.

`ci/install-sbcl.ps1` downloads the official [SBCL 2.6.9 Windows x64 MSI](https://sourceforge.net/projects/sbcl/files/sbcl/2.6.9/),
verifies its pinned SHA256, and extracts it into the job's temporary directory.
The bridge is emitted as `build/wxlambda.dll`, where the Lisp loader expects it;
the wxWidgets DLL directory is added to the test process's `PATH`. Python runs
in UTF-8 mode. This Windows setup has been statically reviewed, not executed on
this macOS host; neither MSI execution nor PowerShell runtime behavior is locally
verified. The full Windows job must pass to establish DLL loading and GUI behavior.

CI uses the exact CFFI, Babel, Alexandria, and trivial-features commits in
`ci/lisp-dependencies.json`, fetched over HTTPS into ignored `.ci-deps/`
checkouts. `ci/setup.lisp` isolates ASDF's source registry from user installations
and stores compiled output in `.ci-deps/fasl/`, separated by Lisp implementation.
This keeps pinned CFFI callbacks separate from Quicklisp's compiled-file cache.
This path requires Git and Python 3, and does not install or modify Quicklisp.
To reproduce the CI Lisp environment locally after building the bridge:

```sh
python3 ci/fetch-deps.py
python3 tests/run.py sbcl --noinform --no-sysinit --no-userinit \
  --non-interactive --load ci/setup.lisp
```

On Linux, prefix the test command with `xvfb-run -a` if no display is available.
CCL can use the same dependencies with:

```sh
python3 tests/run.py ccl --no-init --batch --load ci/setup.lisp
```

For the separate dependency-cache regression, run:

```sh
python3 tests/cache-switch.py
python3 tests/cache-switch.py ccl --no-init --batch
```

Each command runs the callback/handler API scenario with pinned dependencies,
then Quicklisp, then pinned dependencies again in fresh processes. It checks
CFFI source locations and the compiled output destination as well as actual
native callback dispatch. Quicklisp must already be installed; set
`QUICKLISP_SETUP` if its setup file is not `~/quicklisp/setup.lisp`.
These three-process regressions are separate from the 23-scenario GUI suite
and are local checks, not configured hosted jobs.

The fetcher verifies existing checkouts and refuses modified or mismatched ones.
After deliberately changing a locked revision, remove the corresponding generated
`.ci-deps/<name>/` directory before fetching again. Fresh compilation of the
pinned Babel revision emits a warning about `HANDLE-ERROR` in its Korean codec;
the UTF-8 paths exercised by these tests pass on both local Lisps.

Next milestones: hosted platform validation (including the configured Windows
Lisp/GUI tests), more
controls, standalone application packaging, and richer interactive
development support. Establish those contracts before generating larger binding
coverage.
