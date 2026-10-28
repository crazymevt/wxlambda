# Lisp client data attached to managed handlers

ABI 30 adds EVENT-HANDLER-CLIENT-DATA and its generic SETF writer. The receiver
is any current public EVENT-HANDLER wrapper: an owned standalone handler, a
managed window/control or the borrowed managed APPLICATION-HANDLER. The baseline
is wxWidgets 3.2.12. OWNED-EVENT and immutable event snapshots are different
resources and are not handler-client-data receivers.

```lisp
(wxlambda:with-application ()
  (let* ((handler (wxlambda:make-event-handler))
         (state (list :count 0)))
    (unwind-protect
         (progn
           (setf (wxlambda:event-handler-client-data handler) state)
           (assert (eq state (wxlambda:event-handler-client-data handler)))
           (setf (getf state :count) 1))
      (setf (wxlambda:event-handler-client-data handler) nil)
      (wxlambda:destroy-event-handler handler))))
```

## Value identity and native representation

The property accepts any non-NIL Lisp object, retains it strongly and returns the
same object by EQ. It does not copy/serialize the object or snapshot its mutable
contents. Multiple handler associations can refer to the same Lisp value without
making it native-owned memory. SETF returns the supplied value. NIL clears the
bridge association and reads NIL when no bridge datum is present; NIL is not a
distinct stored payload.

A native wxClientData subclass carries only an opaque numeric root token. The
Lisp session root table holds the actual object. No native pointer to a Lisp
object, raw client-data address or generic wxClientData wrapper is exposed.
Native handler ownership deletes the adapter; it does not directly destroy the
Lisp object or call Lisp finalizers. An ordinary Lisp reference to a previously
returned value stays valid independently of the handler association.

Fresh native storage and an empty typed slot read NIL. Replacing or clearing an
association deletes the old native adapter and releases its root through normal
cleanup. A candidate root is established before native assignment and removed if
the write fails, leaving prior native state intact on preflight rejection.
There is no new constructor initarg: a subclass AFTER hook may use SETF once its
native handler exists. The property does not alter handler-enabled state,
bindings, event IDs, stack links or native ownership.

## Foreign storage and typed mode safety

Native wxEvtHandler supports either typed wxClientData or untyped void* storage,
not both. This binding supports only an empty slot or its own typed adapter.
Foreign non-null typed client objects reject get/set without replacement or
deletion. Every untyped/Void mode rejects too, including a slot whose raw pointer
was set to NULL: clearing an untyped pointer does not reset its native mode.
NIL assignment does not grant permission to erase foreign storage or switch
between native modes.

The implementation checks mode before GetClientObject, avoiding native assertions
from calling a typed getter on untyped storage. wxEvtHandler has no public mode
getter. A private implementation interface on the bridge's managed App,
StandaloneHandler and ManagedWindow classes exposes their protected
m_clientDataType solely for this guard. This implementation-header dependency
must be reviewed when advancing the wxWidgets baseline and retained for C7
reconciliation. It is not a new public field/mode getter, arbitrary native
handler adoption or wxClientDataContainer binding.

Untyped GetClientData/SetClientData remains unbound. This property is also not
wxCommandEvent::GetClientObject/SetClientObject or per-item client data on a
choice/list control. Those pointer lifetimes and ownership rules remain gaps.
Per-binding Lisp USER-DATA is now supported separately; it is not this handler slot. Owned-event clone/dispatch rejection of borrowed event client or
userData pointers is unchanged by storing a Lisp value on a handler.

## Lifetime and cleanup boundaries

All accesses require the active main-thread application session and a live
managed receiver. Expired/stale/pending-destruction native access rejects through
existing lifetime guards. Native slot mutation is allowed during callbacks; it
is not a structural handler-chain/stack edit and does not pump native events.

Lisp roots are reaped at client-data access and existing generic binding cleanup
and rollback boundaries; the session root table is cleared after native finish.
Asynchronous native destruction can retain a root until actual deletion followed
by the next cleanup boundary or session end. Do not infer immediate garbage
collection from DESTROY-WINDOW or a weak-reference observation. There is no GC
finalizer, worker access, pointer retention API or cross-session value lookup.

Unlinking a standalone handler, default pop/remove or window destruction that
detaches a pushed handler leaves that handler alive with its own client datum.
Deleting a handler, deleting pop, or actual window/application destruction drops
the associated native adapter; roots are subsequently released as above. Data on
a window and data on its pushed handler are separate associations. Constructor
rollback cleans created native resources and their client roots, without
undoing arbitrary side effects on pre-existing external objects.

Immutable event snapshots do not capture this property. EVENT-SOURCE can identify
a live callback receiver that has handler client data, but saving an event does
not create a separate client-data snapshot or extend native handler lifetime.
Already retained Lisp values remain ordinary valid objects after native cleanup.

Run the example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/handler-client-data.lisp --eval '(wxlambda-handler-client-data:start)'
```

It associates Lisp state, observes it through a callback's receiver, checks EQ
identity and clears the association before disposing resources. Validation must
also distinguish retained values from native pointer payloads, replacement/clear
from foreign-mode overwrite, requested from actual asynchronous deletion, and
managed window data from detached pushed-handler data. Local checks do not
establish hosted Windows/Linux GUI, native OS-session or graphical Cadre behavior.

A separate optional native regression creates otherwise-unreachable foreign
storage states without adding public injection exports. It includes the bridge
implementation in a dedicated test translation unit and checks typed/untyped
rejection with a captured native assertion counter. Build/run in a separate GUI
capable process:

```sh
cmake -S . -B build-native-tests -DWXLAMBDA_BUILD_NATIVE_TESTS=ON
cmake --build build-native-tests --target wxlambda-client-data-native
./build-native-tests/wxlambda-client-data-native
```

The option is OFF by default; this target is development verification, not a
packaged library API or worker-thread facility. Native GUI/display prerequisites
still apply. Require a passing result and zero captured assertions before
claiming safe foreign-mode validation.

Coverage is partial: this supplies managed-handler typed client objects with Lisp
root identity, not arbitrary wxClientData construction, untyped storage, event
client/userData, item data, foreign object adoption or complete client-data
container APIs. Continue those C1 ownership prerequisites explicitly before
widening pointer-bearing APIs.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [handler lifetime](EVENT-HANDLERS.md), [window stacks](WINDOW-HANDLER-STACKS.md),
[owned events](OWNED-EVENTS.md) and [docset installation](DOCSET.md).

[Per-binding Lisp user data](BINDING-USER-DATA.md) now provides BIND-EVENT
:USER-DATA and read-only snapshot/context association access, distinct from
handler client data and native command/client/item pointers. Borrowed native
userdata still prevents owned cloning; no mutable association API is added.
