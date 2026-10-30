# WebView portability audit and acceptance

The requested target is one Lisp WebView interface on macOS WKWebView,
Linux GTK3 WebKitGTK and Windows WebView2 (Edge, not Internet Explorer).
Only macOS has recorded runtime acceptance. Linux's private adapter is
experimental and disabled by default. Windows is **not implemented**.

## Pinned-source findings

Audit target: upstream wxWidgets v3.2.12. Sources:

- [GTK WebKit2 implementation](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/gtk/webview_webkit2.cpp)
- [Windows Edge implementation](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/msw/webview_edge.cpp)

Both upstream RunScriptAsync implementations retain a raw wx owner across
completion. Copying the result into a queued event afterward does not fix
use-after-free before that copy. Linux bypasses this path with native GIO
evaluation and a callback carrying only numeric bridge identities. The GIO
source object remains native-owned; completion never dereferences a wx window.

Windows has an additional **creation-time** hazard: Create registers
OnEnvironmentCreated using raw `this`; that callback registers OnWebViewCreated
the same way. OnEnvironmentCreated also dereferences the environment without
checking the failed HRESULT/null case. GetNativeBackend returns the native
WebView pointer only after asynchronous creation. A completion-safe ExecuteScript
adapter alone therefore cannot make Windows construction or rollback safe.
Do not widen the current construction gate, busy-wait in a nested event loop,
retain dead wx windows forever, or fall back to the legacy IE backend.

## Windows cancellation prerequisite (in progress)

`ci/patch-wx-edge.py` now supplies a narrow, hash-pinned prerequisite patch for
the three upstream Edge source/header files. It validates every input before
writing, supports canonical Windows CRLF checkouts, rejects unknown edits and
partial patch states, and accepts exact reapplication without changes. It copies
the project's GUI-thread-only numeric owner registry into the private wx headers.
It does **not** enable Windows WebView construction in wxLambda.

The patch replaces retained raw-owner completion targets with monotonic numeric
identities. The destructor retires its identity before removing native handlers
and closing the controller. A controller delivered after retirement is closed
without accessing the dead owner. Creation HRESULTs and null interfaces are
checked; creation failures enqueue an error rather than opening a wx log dialog.
Script/user-script completions and the eight native event subscriptions also use
numeric lookup. Callback boundaries contain C++ exceptions; lookup requires the
GUI thread. The registry cannot be copied/moved and identities never wrap/reuse.
This is not a general guarantee for arbitrary foreign wx callback-time deletion.

The official [WebView2 SDK1.0.3179.45 package](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.3179.45)
is staged by `ci/stage-webview2.py`, with a pinned SHA256 and only fixed x64 SDK
files plus licence/notices copied. It rejects differing existing files. This
does not install or validate the WebView2 Runtime. Example prerequisite staging:

```sh
python3 ci/stage-webview2.py vendor/wxWidgets/3rdparty/webview2
python3 ci/patch-wx-edge.py vendor/wxWidgets --check
python3 ci/patch-wx-edge.py vendor/wxWidgets
```

Use an untouched pinned wx3.2.12 checkout. Enable `wxUSE_WEBVIEW_EDGE=1` in its
MSW setup before building wx DLLs; the Windows CI job performs these steps.
`WXLAMBDA_VERIFY_EDGE_COMPLETION_PATCH=ON` then requires Windows/MSVC and
compile-links the patched marker against the actual wx binary. Merely copying
patched headers onto an unpatched library fails that check. Keep the SDK's x64
loader on the process DLL search path when testing the dynamic-loader build.
No system-wide SDK/runtime installation is performed.

The GUI-independent registry regression can be run on any build host:

```sh
cmake -S . -B build -DWXLAMBDA_BUILD_PORTABILITY_TESTS=ON
cmake --build build
ctest --test-dir build -C Release --output-on-failure
python3 -m unittest discover -s tests -p 'test_*.py'
```

The two optional real-input Python integration checks require
`WXLAMBDA_WX_EDGE_SOURCE` (pristine/exact-patched source root) and
`WXLAMBDA_WEBVIEW2_ARCHIVE` (the pinned NuGet package); otherwise they explicitly
skip, while the synthetic rejection/idempotence tests still run. The local
integration used both real inputs; all26 Python tests passed. A C++11 build of
the registry regression also passed with AddressSanitizer/UBSan, including
address reuse, between-stage retirement, late-controller closing and exhaustion.
These checks do not execute COM or replace native Windows build/runtime tests.

The reporting test now derives expected scenario IDs from the runner manifest
and verifies unique IDs, exact ordering and preflight separation instead of
assuming a permanently fixed scenario count.

## Remaining implementation and verification

1. Linux: compile/link against pinned wx3.2.12 and matching WebKitGTK ABI; execute
   the offline example, Lisp WebView suite and native regressions under Xvfb.
   Exercise dialogs, Unicode/JSON, exception results, message limits, immediate
   destruction, pending evaluation destruction and fresh-session late completion.
   Confirm that native teardown disconnects all view-owned signals and downloads.
2. Windows: compile and run the pinned cancellation prerequisite on Windows;
   exercise failed/null creation, destruction before readiness, between creation
   stages, pending script completion and session shutdown. Then add guarded
   backend readiness (no nested wait), WebView2 completion/message adapters,
   copied pre-ready loading policy and native UI suppression before accepting
   application navigation. Public bridge construction remains disabled.
3. Windows UI policy must cover script dialogs, upload, permissions, popups,
   downloads, fullscreen and printing before application navigation. Document the
   WebView2 SDK, loader/runtime deployment and availability diagnostics; test with
   missing runtime as well as installed runtime. Do not claim these tests passed.
4. Preserve ticket/queue bounds, immutable payloads, callback deferral, managed
   initialization rollback and actual-origin/ancestor Input guards on both ports.
5. Record successful per-platform evidence before enabling either new backend by
   default. Keep physical input/IME, arbitrary websites and all OS permission paths
   separate from finite offline acceptance. The disconnected editor REPL cannot
   supply runtime verification; use KryptonEdit's runtime tools when connected.

See [WebView usage and Linux opt-in build](WEB-VIEW.md). This is unfinished
application-driven portability work, not a new exhaustive native coverage goal.
