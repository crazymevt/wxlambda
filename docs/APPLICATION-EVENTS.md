# Application event dispatch

This C1 foundation slice defines dispatch entry and observation for the existing
bridge application. It precedes broader event-handler, timer and worker-dispatch
bindings. It does not introduce arbitrary application or event-loop objects.

All operations require the active GUI session on its process main thread.
Inactive-session calls signal `native-error`, including boolean queries. The
three new APIs enforce this before any FFI call, so it also holds immediately
after loading the Lisp system, before loading the native bridge.

## Observe the application

```lisp
(wxlambda:main-loop-running-p) ; T while the native blocking main loop runs
(wxlambda:pending-events-p)    ; T if wxWidgets has queued pending events
```

`main-loop-running-p` maps to `wxAppConsole::IsMainLoopRunning`. It is false
during standalone `process-events`, `process-pending-events` and the cooperative
application REPL. Those modes can still invoke callbacks; a false query does
not mean it is safe to enter another pump. Inside a `run` callback, the query
reports the native main-loop state.

`pending-events-p` maps to `wxAppConsole::HasPendingEvents`. It observes the
application's wx pending-event handlers, including events queued by `queue-click`.
It does not inspect OS input, idle work or timers that have not queued events.
Its result is an observation rather than a promise about subsequent dispatch.
Both queries return T/NIL and are safe to call from callbacks.

## Dispatch queued wx events

```lisp
(wxlambda:queue-click button)
(assert (wxlambda:pending-events-p))
(wxlambda:process-pending-events) ; returns NIL
```

`process-pending-events` maps to `wxAppConsole::ProcessPendingEvents` across
all pending handlers in the bridge application. It dispatches queued wx events,
without yielding to OS input or running `ProcessIdle`. It does not wait for new
events, but callbacks and continuously requeued work can delay its return.
It is not a replacement for the GUI loop needed to keep an application responsive.

Use `process-events` to yield to GUI events and process idle work, or `run` to
enter the blocking native main loop. `stop` exits that loop; in the cooperative
application REPL it instead ends the REPL. Pending-only dispatch does not create
a native main loop, so `stop` outside RUN/REPL still signals `native-error`.

## Callback and pump boundaries

`run`, `process-events` and `process-pending-events` reject entry while RUN,
either pump, or any native-to-Lisp callback is active. These failures signal
`native-error`; the same protection applies to raw bridge calls. This includes
synchronous command callbacks invoked by a notifying SETF writer before any
main loop starts. Native cleanup is also forbidden from pumps and callbacks.
RAII guards restore dispatch state after native exceptions and callback failures.

Property reads, queries, ordinary window operations, handler replacement and
synchronous notifying setters retain their existing callback behavior. This
boundary prevents recursive event pumping; it does not disable synchronous
command notifications or provide native virtual-method callbacks.

Both pump functions reject dispatch in the application REPL debugger, keeping
the documented pause while debugging. Callback conditions never unwind through
C++; they are recorded and re-signaled when the native operation returns.
Handled boundary failures allow the callback to continue; an escaping condition
uses the existing callback-error recovery rules. A subsequent pump remains usable.

Application dispatch was introduced in ABI 11; the current bridge is ABI 31. Rebuild it alongside the Lisp bindings. This uses
the wxWidgets 3.2.12 inventory baseline; application traits, arbitrary event-loop
objects, generic event binding, pending-event suspension/deletion, filters,
propagation, idle handlers, timers and worker dispatch remain open C1 work.

## Example and checks

The [application-events example](../examples/application-events.lisp) displays
main-loop state from a button callback:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/application-events.lisp \
  --eval '(wxlambda-application-events:start)'
```

The reusable `exercise` invokes that same callback once through explicit pending
dispatch and once through RUN, verifying the different loop states and cleaning
up handlers/windows. The focused scenario also tests reentrancy from pending,
GUI-poll and blocking-loop callbacks, synchronous text notifications, inactive
sessions, debugger pauses and recovery after an escaping callback error:

```sh
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/application-events.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/application-events.lisp
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. The complete GUI runner
includes this scenario. [Install the Cadre docset](DOCSET.md) with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) to browse these APIs, this guide and
the example together.

See the [borrowed application-handler view](APPLICATION-HANDLER.md) for managed wxApp identity, inherited dispatch and lifetime restrictions. Session bindings and logging-off metadata are available; real OS delivery remains unverified.

[Session events](SESSION-EVENTS.md) extend the close family with query/end-session kinds, scoped logging-off metadata and isolated synthetic dispatch. Native OS session delivery remains unverified.

[Pending-event operations](PENDING-EVENTS.md) now distinguish GUI post copies,
consuming native transfer, local copy processing and non-application target
cancellation. Per-handler pending processing and application-wide cancellation
remain unbound.
