# wxlambda

Common Lisp bindings for wxWidgets, using CFFI and a small C++ bridge with a
versioned C ABI. This is an application lifecycle prototype, not a complete
wxWidgets binding.

Implemented: application startup/shutdown, frames, panels, buttons, editable text
controls, vertical sizers, text reading/writing, removable click handlers, and
programmatically queued clicks.
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
search path. The Lisp/native ABI version (currently 2) is checked on load; rebuild
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
- Handlers are zero-argument Lisp functions. `on-click` replaces a previous
  handler and releases its closure. `off-click` removes the current handler and
  releases its closure; repeating it on a live button is harmless. Queued clicks
  use the handler present when they are dispatched, so unbinding suppresses queued
  clicks and rebinding redirects them to the new handler. Unbinding from inside
  the handler itself is supported. Closures for destroyed buttons remain rooted
  until session cleanup unless explicitly unbound before destruction. There are
  no GC-driven native destructors.
- Handler errors stop the loop, are re-signaled in Lisp after the native loop
  returns, and trigger application cleanup when they escape the application body.
  Do not use `throw`, `return-from`, or other nonlocal exits across a callback's
  foreign boundary. Handle expected conditions inside the handler.
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
of a second session. Unicode rendering and manual
mouse input have not been visually verified by these tests.

Validated locally on Apple Silicon macOS with wxWidgets 3.2.12, SBCL 2.6.9, and
CCL 1.13. Windows and Linux are intended targets but have not been tested.

Next milestones: platform CI, event payload access,
more controls, standalone application packaging, and interactive development
support. Establish those contracts before generating larger binding coverage.
