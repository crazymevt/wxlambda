# Registered command and notify events

This C1 slice binds `wxNewEventType`, synthetic `wxCommandEvent`/`wxNotifyEvent`
families and synchronous `wxEvtHandler::ProcessEvent`. The baseline is wxWidgets
3.2.12 and the native bridge is ABI 23. It does not bind native control-specific
notify subclasses or OS session routing.

## Register a named native type

```lisp
(wxlambda:register-event-type :before-apply :family :notify) ; :BEFORE-APPLY
(wxlambda:event-type :before-apply)                        ; native integer ID
(wxlambda:register-event-type :result-ready :family :command)
```

Names must be unique keywords in the active main-thread session. The default
family is `:notify`; `:command` is also supported. Built-in kinds cannot be
redefined, and duplicate names or unsupported families are errors. Registration
does not replace existing handlers or type definitions. There is no unregister
operation. Names and bridge mappings last until session cleanup; native IDs are
allocated by wxWidgets and must not be treated as stable serialized identifiers.
Register each name once per session before binding or dispatching it.

Type declarations are session-wide metadata, not owned resource allocations.
If an initializer registers a type and later fails, that declaration is not
rolled back; managed rollback still removes its native handlers, windows and
callback registrations. Saved snapshots retain copied kind, family and native
type data after the registry/session ends.

## Validate synchronously with Veto and Allow

```lisp
(defvar receiver (wxlambda:make-event-handler))
(wxlambda:bind-event receiver :before-apply
  (lambda (event context)
    (setf (wxlambda:event-vetoed-p context)
          (minusp (wxlambda:event-integer event))))
  :with-context t)

(multiple-value-bind (handled vetoed)
    (wxlambda:process-event receiver :before-apply :integer -1 :text "Proposed change")
  (when (and handled (not vetoed))
    ;; Apply the change here, after validation.
    (format t "Accepted~%")))
```

`process-event` creates a fresh native event, dispatches it synchronously and
returns two values: native **handled-p** and final **vetoed-p**. These are
independent. Observer callbacks can run while handled-p is NIL because they Skip;
it reports native consumption/default handling, not whether a callback ran.
An initial veto can remain true even if no binding matched. The example uses a
consuming contextual policy handler and requires both handling and no veto before
applying the proposed change.

Notify contexts support veto intrinsically: `event-can-veto-p` returns T.
`event-vetoed-p` reads `NOT IsAllowed`; SETF true calls `wxNotifyEvent::Veto`, and
SETF NIL calls `Allow`. This is distinct from close-event `CanVeto` restrictions,
which remain checked. Neither notify veto nor Allow changes Skip. Contextual
bindings still begin unskipped; observers still automatically Skip.

Snapshots capture veto state before their callback. They do not change after a
context calls Veto/Allow. The existing innermost-callback checks, nested context
suspension and expiration rules apply; saved contexts cannot mutate a later event.

## Payloads and asynchronous queuing

Registered command and notify types accept:

- `:id`: signed 32-bit command identifier, default 0, excluding reserved -1.
- `:text`: copied UTF-8 string, default empty; Unicode and embedded NULs survive.
- `:integer`: signed 32-bit value, default 0, copied from native `GetInt`.
- `:vetoed-p`: initial generalized-boolean veto, default NIL, notify family only.

`queue-event` accepts the same payloads and returns the supplied receiver, but
offers no synchronous final result. Mutating the original Lisp string after the
call does not change the queued event. `event-integer` reads the copied command
integer in saved snapshots; close snapshots report zero. Built-in toggle and
selection events keep their specialized payload accessors. Custom kinds use
`event-integer`, not the built-in `event-selection` slot.

`event-family` now reads stored `:command`, `:close` or `:notify` classification;
it does not consult a live registry. Registered types work with existing ID
filters, independent bindings, safe unbinding, disabled-handler behavior and
borrowed window identities. They do not automatically change a control or apply
the proposed operation. Use the returned decision to implement application policy.

## Dispatch lifetime and errors

The bridge owns the temporary native event during synchronous dispatch, copies
the final veto flag and deletes the event after dispatch returns. Deferred
receiver disposal during its callback does not invalidate that event or its
still-running context. Native callback objects remain pinned across unbinding.
Managed initialization failure removes callback roots and registrations for its
constructed resources.

Callback errors are re-signaled at the `process-event` Lisp boundary, so handling
a nested error need not stop an enclosing RUN. No result values are returned when
a condition escapes; earlier handler mutations are not rolled back. An error is
not an implicit veto. Existing [close-event default actions](CLOSE-EVENTS.md)
also apply when a close is synchronously dispatched. Synchronous dispatch is not
an event pump; nested calls are permitted with context isolation, while recursive
RUN/poll entry remains forbidden. All calls are main-thread/session restricted.
Worker-to-GUI dispatch is not supplied here.

## Session and remaining family prerequisites

Native query/end-session events belong to `wxApp`, not a window handler. The
default end-session action may exit the process. A borrowed application handler
view and session-family bindings/metadata are now available; synthetic session
events require isolated owned handlers and never reach wxApp fallback.
Real OS delivery remains unverified. See [session events](SESSION-EVENTS.md).
Tests do not emit real OS logout/shutdown events.

Native control-specific notify subclasses, arbitrary native class registration,
userData, persistent event ownership/cloning, remaining native payload fields,
context text/integer mutation, pushed/linked handlers, timer/idle and worker
dispatch remain required coverage work. The registered base notify mapping does
not complete those classes or the full C1 phase.

## Example and verification

The [notify-events example](../examples/notify-events.lisp) registers a notify
policy, vetoes while a checkbox blocks changes, then Allows and applies a change.
Its reusable `exercise` invokes the same button callback in both cases:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/notify-events.lisp \
  --eval '(wxlambda-notify-events:start)'
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/notify-events.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/notify-events.lisp
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper in agent sessions. Focused tests cover
registration conflicts, synchronous handled/veto distinctions, initial-veto Allow,
integer bounds, copied NUL/Unicode text, saved registry-independent snapshots,
disposal, initializer rollback and nested error recovery while RUN continues.
The API, guide and example are indexed in the [Cadre docset](DOCSET.md), installable
under `$XDG_DATA_HOME/docsets/wxlambda/` (default `~/.local/share/docsets/wxlambda/`).
Hosted GUI and graphical Cadre remain separate validation work.

See the [borrowed application-handler view](APPLICATION-HANDLER.md) for managed wxApp identity, inherited dispatch and lifetime restrictions. Session-event routing remains unbound.
