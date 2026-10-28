# Rooted Lisp user data on individual bindings

ABI 30 adds `:user-data` to BIND-EVENT and the read-only EVENT-USER-DATA generic.
The baseline is wxWidgets 3.2.12. User data belongs to one registration, not to
the receiving handler as a whole or to a reusable owned-event payload.

BIND-EVENT accepts any Lisp object; NIL means no association. The value is
retained by EQ with the binding's callback roots. It does not alter callback
arity: observers still receive one immutable snapshot, and `:with-context t`
callbacks still receive `(snapshot context)`. Retrieve the value with
EVENT-USER-DATA instead of expecting an extra callback argument.

```lisp
(let* ((state (list :count 0))
       (binding
         (wxlambda:bind-event receiver :message
           (lambda (event context)
             (assert (eq state (wxlambda:event-user-data event)))
             (let ((value (wxlambda:event-user-data context)))
               (incf (getf value :count))))
           :with-context t :user-data state)))
  (unwind-protect
       (wxlambda:process-event receiver :message)
    (wxlambda:unbind-event binding)))
```

The receiver/kind must already be valid and `:message` registered as appropriate.
The complete runnable example supplies that setup and cleanup.

## Immutable association, ordinary mutable Lisp value

A snapshot captures the association before its callback. EVENT-USER-DATA on that
snapshot needs no native session and remains usable after unbinding, receiver
cleanup or session end. It retains the same Lisp reference, not a deep copy.
Mutable object contents can change and those changes are visible through other
references and earlier snapshots. Immutable snapshot structure does not freeze
an arbitrary list/hash-table/object stored inside it.

No SETF EVENT-USER-DATA or binding-data mutation API is supplied. To replace the
association, unbind and create a new registration. Existing snapshots retain the
old reference; another binding can use a different value on the same receiver,
kind and callback function.

For generalized places such as GETF, first bind the reader's result to a local
variable as above. Direct `(incf (getf (event-user-data context) :count))` can
require a nonexistent SETF EVENT-USER-DATA even when modifying existing plist
contents. Assigning a new object to that local variable does not replace the
binding association; mutate the existing object or rebind explicitly.

## Current-context lifetime and self-unbinding

Context reads require an active main-thread session and the innermost current
callback. The native context captures the binding identity before invoking Lisp;
a local Lisp dispatch frame retains its value until callback return. Self-unbind,
peer-unbind or garbage collection therefore cannot invalidate the active
association merely by removing its global binding roots.

The reader does not dereference a binding userData pointer after unbinding has
deleted its native adapter. Nested dispatch suspends outer context access;
after it returns, the outer association is restored. Expired/suspended contexts
signal NATIVE-ERROR even when the data itself is an ordinary still-live object.
Retaining a context does not grant future access; retain a snapshot/value instead.

Unbinding, receiver deletion and session cleanup release binding roots according
to the existing callback/lifetime rules. Captured snapshots, active frames and
ordinary application references may independently keep the Lisp object alive.
No immediate garbage-collection timing, worker-thread binding or cross-session
native callback is promised.

## Native ownership and the three separate data mechanisms

A native wxObject adapter stores only an opaque numeric binding token. The
wxWidgets Bind entry owns/deletes that adapter on disconnect/teardown. The Lisp
binding/dispatch roots hold the actual Lisp value; no Lisp address or arbitrary
native pointer is exported. This is a prerequisite use of wxObject, not a public
generic wxObject factory/adoption interface.

- EVENT-USER-DATA is per-binding callback association, captured into snapshots.
- EVENT-HANDLER-CLIENT-DATA is a separate handler-wide EQ value in a wxClientData
  token adapter. Changing it does not replace any binding user data.
- wxCommandEvent client data/client object and control item data are still
  separate native payload/ownership APIs and remain unbound.

Live OWNED-EVENT reads validate the resource and return NIL because safe owned
events do not carry borrowed binding pointers. A non-null borrowed pointer
rejects rather than being adopted. No user-data constructor or synthetic
queue/process keyword payload is added; data comes from the matching binding
when delivery occurs. Existing convenience ON-* registrations do not gain a
USER-DATA keyword; their snapshots report NIL.

CLONE-EVENT still rejects a current event carrying non-null borrowed native
binding userData, including after self-unbind. The guard only tests pointer
presence and does not dereference a deleted adapter. A saved snapshot/value is
the supported way to retain that Lisp association; an owned native clone does
not adopt it. NIL-data bindings leave the prior supported clone behavior intact.

Run the example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/binding-user-data.lisp --eval '(wxlambda-binding-user-data:start)'
```

The optional native invariant target also checks captured context identity after
self-unbinding, expired context rejection and owned borrowed-pointer guards:

```sh
cmake -S . -B build-native-tests -DWXLAMBDA_BUILD_NATIVE_TESTS=ON
cmake --build build-native-tests --target wxlambda-client-data-native
./build-native-tests/wxlambda-client-data-native
```

It is OFF by default, requires a native GUI-capable process and injects internal
states without public pointer APIs. Require zero captured native assertions.
Local validation does not establish hosted Windows/Linux GUI, real OS-session
or graphical Cadre behavior. General native userData adoption, mutable binding
associations, untyped/client/item payload ownership and arbitrary wxObject
wrappers remain C1 work.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [bindings](EVENT-BINDINGS.md), [handler client data](HANDLER-CLIENT-DATA.md),
[owned events](OWNED-EVENTS.md) and [docset installation](DOCSET.md).
