# Worker-to-GUI thread events

ABI58 adds `queue-thread-event`, mapping copied string delivery to native `wxThreadEvent` and
thread-safe `wxEvtHandler::QueueEvent`. The build requires `wxUSE_THREADS=1`;
disabling native wx locking is unsupported. Its receiver is an existing managed
EVENT-HANDLER, including a window or standalone handler. Establish its internal
posting endpoint by binding `:thread` on the GUI thread before starting a worker:

```lisp
(wxlambda:bind-event receiver :thread
  (lambda (event) (format t "Worker sent: ~A~%" (wxlambda:event-text event))))
;; In a Lisp worker thread:
(wxlambda:queue-thread-event receiver "Finished λ 😀")
```

The call returns RECEIVER after copying/enqueueing the payload. It accepts a
string of at most1MiB UTF-8, including embedded NUL; malformed native UTF-8
rejects. A posting worker may reuse/mutate its source after return. Do not mutate
the same Lisp string concurrently while it is being encoded. Native code retains
an independently owned native wxThreadEvent string, never the Lisp string,
closure, wxAny value or raw user pointer. The worker constructs/uniquifies that
native string and destroys all temporary wxString aliases before queue
publication. Native GetString therefore reports the actual payload; native
wxThreadEvent::Clone deep-copies it without copying queue-accounting reservations.
The public owned-event clone API still rejects thread events.
This operation does not create or manage Lisp worker threads.

Only `queue-thread-event` is worker-safe. Binding/unbinding, receiver creation,
GUI property access, event-loop entry, destruction and ordinary `queue-event`
retain their GUI-thread/session rules. The posting path does not consult the
ordinary GUI/session registries, call GUI-ready validation or invoke Lisp GUI
callbacks. No new public endpoint or editor/task abstraction is introduced.

## Delivery and lifetime

`:thread` is a native base event (`event-family` `:base`, native THREAD category),
not a command event. `event-text` is copied into an immutable snapshot and may
also be read from the current innermost borrowed context. Payload SETF and
owned/synthetic/cloned thread events reject. Observers automatically Skip;
contexts retain the existing explicit Skip/propagation rules. Thread events do
not automatically bubble through window parents like command events.

Delivery occurs only through GUI event dispatch. Workers never run the binding
callback. Existing callback lifetime/context protections apply. Snapshot text
remains usable after receiver/session teardown. Native QueueEvent wakes the
GUI loop; it does not run the receiver synchronously on the posting thread.
An accepted post is not an acknowledgment that a binding eventually ran.

The internal endpoint survives unbinding until the native receiver dies. Posts
to a previously registered receiver with no binding can be accepted and ignored
by native dispatch. Before initial binding, unknown/stale handles and closed
session ingress reject with NATIVE-ERROR. Native Destroy retires the endpoint
before native cleanup, with weak-safe restoration on refusal/exception. Native
destruction unregisters the
endpoint while excluding concurrent queue operations. A post accepted just
before deletion/pending destruction may be discarded. Frame Close acceptance
does not itself guarantee native destruction. Receiver identities are monotonic;
worker copies of stale receiver objects do not resurrect native resources.

At most256 live queued/delivering packets per receiver and4096 per session are
accepted; each is at most1MiB. Full queues reject with NATIVE-ERROR. Count
reservations retire when native packets are deleted, including discarded or
unhandled packets. There is no automatic coalescing or guaranteed delivery.
FIFO follows serialized native posting order to one receiver, not wall-clock
order between concurrent producers. Existing `delete-pending-events` can discard
packets; deletion and wx pending-handler unregistration serialize with posts.

Session shutdown closes ingress before binding/receiver/native cleanup. A post
already inside the queue critical section completes before shutdown proceeds;
later posts reject without entering wx. Workers need not call GUI APIs to stop
posting and no worker termination is awaited under the ingress mutex. The
application still owns thread termination/joining. Posting after session cleanup
is safe and rejects; native shutdown does not silently launch another session.

## Verification scope

The example/tests use real SBCL/CCL threads to post Unicode/NUL text and verify
GUI-only callback delivery and immutable copies. Native fixtures cover count and
byte limits, invalid UTF-8, unbound delivery, pending deletion, target-destruction
and shutdown races, and stale posts after cleanup. No physical input, external
network or system clipboard transfer is involved.

See [thread event example](../examples/thread-events.lisp),
[event contexts](EVENT-CONTEXTS.md) and [deferred UI actions](DEFERRED-UI-ACTIONS.md).

## Pinned native lock audit

The internal ingress mutex serializes posting, receiver retirement/destruction
and pending-event deletion. It is never held while invoking Lisp or running a
native Destroy body. Refused/throwing Destroy restores the endpoint only while
the same weak-tracked receiver survives. Packet accounting uses atomics so
discard/destruction never takes the ingress mutex under a wx pending-event lock.

Pinned [wxAppConsoleBase::ProcessPendingEvents](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/common/appbase.cpp#L563)
releases its global pending-handler lock before calling a receiver.
[wxEvtHandler::ProcessPendingEvents](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/src/common/event.cpp#L1382)
releases the receiver pending lock before calling its event handlers. Therefore
GUI callbacks can retire a receiver without reversing the posting lock order.
The native DeletePendingEvents operation itself lacks that lock; the bridge
serializes deletion plus pending-handler unregistration against worker posts.
Re-audit these rules before widening to a different native lock implementation.
Arbitrary allocator failure inside wx's own critical sections is outside the
recovery guarantee; normal invalid/full/stale/shutdown errors occur before wx.
