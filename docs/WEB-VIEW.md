# Interactive WebView for editor development

ABI56 adds `web-view`, a native-backed WINDOW container owning a private
wxWebView browser. The initial backend is explicitly gated to macOS,
wxWidgets3.2.12 and WKWebView. Except for the explicit experimental Linux opt-in
below, other ports/versions reject construction until their asynchronous lifetime
and native UI paths are audited and tested. Building requires
the wxWidgets `webview` component; macOS also compiles an Objective-C++ adapter
and links the system WebKit framework. JavaScript is enabled.

### Cross-platform extension status

Linux GTK3 has an **experimental, opt-in** WebKitGTK adapter, not yet verified
on a Linux host. CI with SBCL/WebKitGTK2.52.6 builds successfully but aborts in
JavaScriptCore's stack sanitizer during real execution. A pre-construction
pthread-stack compatibility check rejects alternate native stacks before entering
that VM. This contains the crash; it is not completed SBCL/Linux WebView support.
Do not disable JavaScriptCore assertions or main-thread guards as a workaround.
Run41155287791 confirmed rejection by the new preflight: the current native
stack is outside pthread's range on that SBCL host. WebKitGTK2.52.6 checks saved stack tops against pthread
bounds in [VM.cpp](https://github.com/WebKit/WebKit/blob/webkitgtk-2.52.6/Source/JavaScriptCore/runtime/VM.cpp)
and derives those bounds in [StackBounds.cpp](https://github.com/WebKit/WebKit/blob/webkitgtk-2.52.6/Source/WTF/wtf/StackBounds.cpp).
Windows construction remains unavailable: adding Windows is
not complete. See [the backend audit](WEB-VIEW-PORTABILITY.md) for the concrete
Windows lifetime prerequisite patch and the remaining acceptance gates.

Linux requires wxWidgets **3.2.12**, built with GTK3/WebKit2, WebKitGTK >=2.22
and GLib >=2.66. The adapter must link the **same WebKitGTK ABI** as wxWidgets;
4.0 and 4.1 must not be mixed in one process. Configure, for example, when wx
was built against 4.1:

```sh
cmake -S . -B build -DWXLAMBDA_EXPERIMENTAL_GTK_WEBVIEW=ON \
  -DWXLAMBDA_WEBKITGTK_PACKAGE=webkit2gtk-4.1
cmake --build build
sbcl --load ~/quicklisp/setup.lisp --load tests/webview.lisp
```

Configuration runs a disposable linked-process ABI probe and refuses cross
compilation. Mixed libsoup dependencies can abort at load time, so the later
in-process ABI check is only defense in depth. Do not combine a packaged bridge
with a different wx/WebKit ABI from the one used to build and probe it.

The Lisp and JavaScript interfaces below are unchanged on Linux, including
`window.webkit.messageHandlers.wxlambda.postMessage`. The adapter calls native
WebKitGTK asynchronous evaluation directly; it does not use wx's raw-owner
`RunScriptAsync`. Completion data contains numeric owner/ticket IDs only.
It suppresses script dialogs, file choosers, popups, fullscreen and printing,
denies permission/authentication requests and cancels view-owned downloads.
These paths and immediate/pending teardown still require Linux runtime tests.
This is not a security sandbox; do not use the opt-in for untrusted browsing.

```lisp
(defparameter preview (wxlambda:make-web-view parent))
(wxlambda:bind-event preview :web-message
  (lambda (event) (format t "Page sent JSON: ~A~%" (wxlambda:event-text event))))
(wxlambda:bind-event preview :web-script-result
  (lambda (event)
    (format t "Request ~D, success ~D: ~A~%"
      (wxlambda:event-extra-long event) (wxlambda:event-integer event)
      (wxlambda:event-text event))))
(setf (wxlambda:web-view-url preview) "http://localhost:3000/")
;; After the application reports readiness:
(wxlambda:run-web-script preview "({title:document.title, passed:2+2===4})")
```

From JavaScript, send JSON-compatible data back to Lisp:

```javascript
window.webkit.messageHandlers.wxlambda.postMessage({type: "test-result", passed: true});
```

Messages are data. The binding does not parse JSON into Lisp objects or evaluate
received strings as Lisp code. Use an application JSON library and explicit
message handling. All frames/origins can send messages: receipt does not
authenticate the sender. This is a trusted development preview, not a browser
security sandbox or a full browser automation/DevTools protocol.

## Construction and page loading

`make-web-view` requires a live WINDOW parent and accepts validated `:class`
and subclass initargs. Direct `make-instance` supports slots/default initargs,
native properties in AFTER hooks and managed construction/root rollback.
The initial page is `about:blank`. Lisp ancestry is WINDOW, not PANEL. Existing
row/column layout accepts the view with `:proportion 1 :expand t`; the private
browser child and its own sizer are not exposed. Managed handler stacks reject.

`web-view-url` reads the native current URL; SETF requests asynchronous
navigation and returns its supplied string. It need not immediately read back
that URL. Input must be a nonempty absolute `http`, `https` or `file` URL, or
exactly `about:blank`. URL inputs are at most8192 UTF-8 bytes, NUL-free and reject
whitespace/control characters and malformed percent escapes. HTTP(S) requires
a host. Encode URL spaces as `%20`; use `run-web-script` for JavaScript rather
than `javascript:` navigation.

`set-web-page` takes HTML and optional `:base-url` (default `"about:blank"`),
returns the supplied HTML string and requests asynchronous loading. HTML is
NUL-free and at most2MiB UTF-8; the base follows the same URL rules. Input strings
are copied natively. Accepted requests are not successful-load guarantees.
Loading uses an Objective-C exception boundary so malformed URLs become
NATIVE-ERROR instead of unwinding through Lisp. Redirects, page-driven links and
subresources follow WebKit; the initial URL validator is not a navigation sandbox.

`web-view-title` returns a fresh native title string. `web-view-busy-p`,
`web-view-can-go-back-p` and `web-view-can-go-forward-p` return native booleans.
`web-view-go-back`, `web-view-go-forward`, `web-view-reload` and `web-view-stop`
return the view after requesting their native operation; unavailable history
directions reject. Stop does not roll back a page or cancel outstanding script
evaluations. Network, local-server, certificate and WebKit policies remain native.

## Asynchronous JavaScript and events

`run-web-script` accepts a NUL-free script of at most1MiB UTF-8 and immediately
returns a positive request ticket. It never spins a nested synchronous browser
loop. At most64 requests can be outstanding per view, including results queued
for delivery. Tickets increase monotonically and are bounded to2147483647.

Use `bind-event` on the view or its ancestors:

| Kind | `event-text` | Other payload |
| --- | --- | --- |
| `:web-message` | JSON serialization of the posted message body | no authenticated origin/frame metadata |
| `:web-script-result` | JSON result on success; native error text on failure | `event-extra-long`: ticket; `event-integer`:1 success,0 failure |
| `:web-loaded` | Native reported URL | may occur for native loads before application readiness |
| `:web-error` | Native load error description | does not imply a script request failed |

JSON fragments are supported: strings are quoted, arrays/objects/numbers and
booleans serialize normally; undefined/nil becomes `null`. Nonserializable native
results and JavaScript exceptions produce error results. Promise objects are
not awaited; use page messaging from asynchronous application code. Result JSON
is at most1MiB; an oversized result becomes an error result. Incoming messages
over1MiB or beyond256 queued messages per view are discarded. Avoid flooding the
bridge. Native load notifications are informational, not an application test
readiness protocol. Use an explicit ready message or application predicate.

Events carry copied immutable snapshots and optional current-innermost borrowed
contexts. Text/integer/extra-LONG payload SETF rejects for these kinds; Skip and
propagation remain supported. No owned/synthetic/cloned web events are exposed.
Scripts and messages may finish after navigation: tickets identify requests,
not document generations. Correlate application/page IDs in your own messages.

Native callbacks only enqueue copied wx events. Lisp delivery occurs through
normal event dispatch with receiver/ancestor lifetime protection. Script
completion blocks retain numeric owner/ticket IDs, never raw C++ window pointers.
Owner/session destruction clears requests, removes the message handler and drops
late results. A view is not kept alive by an outstanding JavaScript request.

All calls require the GUI thread and live session. Page/navigation/script writes
reject event callbacks; use owner-bound `defer-ui-action`. Reads are callback-safe.
Nested pumps/modal/clipboard entry and receiver/ancestor destruction reject during
web callbacks. Run the normal native loop (`run` or the interactive REPL loop),
or full `process-events` polling; pending-event dispatch alone is not a browser
run loop. The example's bounded tests use `run` with a UI timer.

## Native UI policy and tested scope

The WK adapter acknowledges alert, returns false for confirm and null for prompt,
cancels file upload panels, blocks new windows, disables context menus and
suppresses the pinned native printing hook. Supported media/geolocation requests
are denied through SDK-gated delegate methods. The injected wx fullscreen
script/message handler is removed. These measures prevent known unguarded wx
modal paths; they do not claim complete WebKit permissions/security policy.
Native browser keyboard/clipboard interactions follow WebKit and are separate
from the library's explicit clipboard APIs. Automation does not transfer system
clipboard content.

[The runnable editor preview](../examples/webview.lisp) provides a JavaScript
text area, Run button, live browser and JSON result output. Tests load only
embedded offline HTML, run actual JavaScript, exchange Unicode JSON, contain
alert/confirm/prompt, check malformed URLs, callback propagation/metadata guards,
subclass rollback and pending-result disposal. Native regressions add raw bounds,
thread/queue limits and late-result rejection. Physical browser input, arbitrary
network/auth/certificate flows, file-upload gestures, every OS permission path,
DevTools integration, hosted platforms and graphical Cadre remain unverified.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/webview.lisp \
  --eval '(wxlambda-webview:start)'
```

See [grid](GRID.md), [CLOS API](CLOS-API.md), [deferred actions](DEFERRED-UI-ACTIONS.md)
and [Cadre installation](DOCSET.md).
