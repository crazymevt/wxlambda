# wxlambda

Common Lisp bindings for wxWidgets, using CFFI and a small C++ bridge with a
versioned C ABI. This is an application lifecycle prototype, not a complete
wxWidgets binding.

Implemented: application startup/shutdown, frames, panels, buttons, editable text
controls, vertical sizers, text reading/writing, removable click and text-change
handlers, copied event payloads, and programmatically queued clicks.
Lisp objects wrap checked integer handles; native pointers never escape the bridge.

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
search path. The Lisp/native ABI version (currently 3) is checked on load; rebuild
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

| Accessor | Click event | Text-change event |
| --- | --- | --- |
| `event-kind` | `:click` | `:text-change` |
| `event-source` | Original button wrapper | Original text-control wrapper |
| `event-text` | `nil` | Copied Lisp string, including `""` for empty text |

Text-change handlers always take one event argument. Typing into the native text
field emits events. `set-text` stays silent by default to avoid update loops;
`:notify t` uses wxWidgets' notifying setter and may call the handler immediately,
before the setter returns. Handlers can therefore run before `run` starts.

The text payload describes that event, rather than a later read of the control.
It remains valid across nested updates, handler unbinding, and window destruction.
Event text is an independent Lisp string; callers can retain or modify their copy.
The source wrapper remains available, but native operations on it fail after its
window is destroyed. Event slots do not expose any borrowed native pointers.

Use `off-click` or `off-text-change` to release a handler. Registering again
replaces and releases the previous closure. Both removals are idempotent for a
live control of the correct type and may be called inside the handler itself.

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
  `:with-event t`. Text-change handlers take one event. Handler replacement or
  explicit unbinding releases the registered closure. Queued clicks use the
  handler present when they are dispatched, so unbinding suppresses queued clicks
  and rebinding redirects them to the new handler. Closures for destroyed controls
  remain rooted until session cleanup unless explicitly unbound before
  destruction. There are no GC-driven native destructors.
- Errors in synchronous text-change callbacks are saved until the notifying
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
- One GUI session per process; no application restart, interactive REPL event-loop
  integration, background-thread GUI scheduling, or virtual-method overrides yet.

The bridge catches native exceptions at exported operation boundaries and exposes
error text through `wxl_last_error`. Its text is borrowed until the next bridge
operation on the same thread. See `native/wxlambda.h` for the C interface.

## Verification

These tests briefly open real windows, queue a button event, then exit. They need
an interactive graphical session (or a suitable display server on Linux).
Each scenario runs in a fresh process with a 30-second timeout:

```sh
python3 tests/run.py
python3 tests/run.py ccl --no-init --batch --load ~/quicklisp/setup.lisp
```

Coverage: callback dispatch and replacement, empty/long/Unicode text round trips,
independent text copies, explicit closure release, queued click suppression,
rebinding and self-unbinding, parent/child invalidation, wrong-type handles,
duplicate sizer insertion, cleanup on setup and callback errors, and rejection
of a second session. Event tests cover source/kind metadata, copied Unicode and
empty text payloads, nested updates, silent versus notifying setters, self-unbinding,
and text-handler errors before and during the loop, including caught errors that
must leave the loop running. Unicode rendering, manual
mouse input, and keyboard-generated text events have not been visually verified
by these tests; text events are triggered using the native notifying setter.

Validated locally on Apple Silicon macOS with wxWidgets 3.2.12, SBCL 2.6.9, and
CCL 1.13. Windows and Linux are intended targets but have not been tested.

## Continuous integration

`.github/workflows/ci.yml` runs on pushes, pull requests, and manual dispatch.
The configured jobs are:

| Runner | Native build | Lisp GUI tests |
| --- | --- | --- |
| Ubuntu 24.04 | Distribution wxWidgets 3.2, GCC | SBCL, eight scenarios under Xvfb |
| macOS 15 | Homebrew wxWidgets 3.2, Clang | SBCL, eight scenarios |
| Windows Server 2022 x64 | wxWidgets 3.2.12 from source, MSVC | Not configured yet |

These are configured checks, not a claim that the hosted jobs have passed. No
hosted runs were available when the workflow was added. The Windows job checks
compilation and linking only; it does not validate DLL loading, callbacks, or the
GUI lifecycle. CCL remains locally tested on macOS, outside this CI matrix.

The Windows build follows the upstream [MSVC makefile instructions](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/docs/msw/install.md).
It builds wxWidgets release DLLs from a pinned commit, then passes explicit
`wxWidgets_ROOT_DIR`, `wxWidgets_LIB_DIR`, and `wxWidgets_CONFIGURATION=mswu`
settings to CMake. System package versions on Linux/macOS follow their package
repositories; these native dependencies are not locked to an exact patch release.

CI uses the exact CFFI, Babel, Alexandria, and trivial-features commits in
`ci/lisp-dependencies.json`, fetched over HTTPS into ignored `.ci-deps/`
checkouts. `ci/setup.lisp` isolates ASDF's source registry from user installations.
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

The fetcher verifies existing checkouts and refuses modified or mismatched ones.
After deliberately changing a locked revision, remove the corresponding generated
`.ci-deps/<name>/` directory before fetching again. Fresh compilation of the
pinned Babel revision emits a warning about `HANDLE-ERROR` in its Korean codec;
the UTF-8 paths exercised by these tests pass on both local Lisps.

Next milestones: hosted platform validation, Windows Lisp/GUI testing, more
controls, standalone application packaging, and interactive
development support. Establish those contracts before generating larger binding
coverage.
