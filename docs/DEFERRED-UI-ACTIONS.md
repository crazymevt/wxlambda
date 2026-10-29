# Deferred UI actions

ABI43 adds a session-owned FIFO queue for main-thread UI work after native
callbacks. Use it when a menu, toolbar, accelerator or navigation callback needs
to open a modal dialog, transfer clipboard text or destroy/restructure windows.
Existing direct callback guards remain in force.

```lisp
(wxlambda:bind-event frame :menu
  (lambda (snapshot context)
    (declare (ignore snapshot context))
    (wxlambda:defer-ui-action
      (lambda ()
        (let ((dialog (wxlambda:make-file-dialog :parent frame)))
          (unwind-protect
               (when (eq :ok (wxlambda:show-modal dialog))
                 (setf (wxlambda:status-text status)
                       (wxlambda:file-dialog-path dialog)))
            (wxlambda:destroy-window dialog))))
      :owner frame))
  :id 32101 :with-context t)
```

`defer-ui-action` takes a function of no arguments and optional `:owner` window,
and returns an immutable `ui-action` ticket. Scheduling never calls the function
inline. At most 1024 pending actions are supported. The function is strongly
rooted by the session until delivery, cancellation, observed owner expiry or
session cleanup; a local root retains an executing function across modal callbacks
and garbage collection. `ui-action-p` identifies tickets; no public constructor,
token accessor, raw callback pointer or native resource adoption is exposed.

## Delivery and cancellation

Actions run in FIFO order, at most one per safe native idle iteration, after
wxApp::ProcessIdle and its callbacks return. A single `process-events` call may
perform several idle iterations and deliver several actions. `run` and
`process-events` drive delivery. `process-pending-events` deliberately does not
run this queue; pending-event queries describe wx events, not queue length.
Idle availability determines timing: there is no deadline/timer or background
worker. Long action functions still occupy the GUI thread.

Native idle/bridge-entry depth gates prevent early delivery inside the originating
callback or a still-running synchronous native operation, even if it processes
idle recursively. Delivery waits while a modal/dialog/clipboard or navigation/
tool/accelerator operation is active, while another action runs, during shutdown
or while a callback error is pending. A modal opened by one action cannot deliver
following actions in its nested event loop. Use END-MODAL directly from an
appropriate current dialog callback to finish an existing modal: deferred actions
wait for modal exit and cannot dismiss that active dialog. Actions enqueued by an executing
action join the tail and never run recursively. An idle wake requests work without
busy requeueing blocked actions during modal loops.

`ui-action-pending-p` is true only while the ticket is queued and its optional
owner is live. It is false during execution and after completion/cancellation.
`cancel-ui-action` removes queued work and returns T once, NIL if no entry remains;
it cannot interrupt active work. It can remove an expired owner entry before a
query/prune has already removed it. A ticket retains its type after completion.
Both operations require the active session and main thread.

`:owner` is checked before delivery through a native weak reference; native
window ownership and pending ancestor deletion count. It does not keep a widget
alive or grant protection to an executing function. Expired owners suppress the
function. Their Lisp roots are released when expiry is observed/pruned, at safe
idle delivery or session cleanup, rather than synchronously in a native destructor.
Without an owner, work belongs to the session and can outlive individual frames.
Validate any other captured widget identities when needed. Capture immutable event
snapshots/values, not borrowed contexts: old event contexts are expired in actions.
Current UI values are read at delivery unless the closure captured earlier values.

## Safe action phase

The action itself is outside native event callback guards. Modal/clipboard access,
window destruction and previously quiescent structural operations are allowed
from this safe phase even while the surrounding RUN/poll driver is active.
This includes menus, frame bars, accelerator tables, notebook/splitter attachment,
managed handler chains/stacks and deleting managed pending events. Normal type,
ownership, initialization and native lifetime checks still apply. Newly nested
event callbacks receive their usual guards; the exception belongs only to the
outer action body. Clipboard and modal APIs retain their own context suspension.

Calling RUN, either event-pump API or session finish recursively from an action
rejects. `stop` may exit an active RUN; it does not cancel the session's remaining
queue. Outside actions, structural operations retain their prior quiescent rule.
Do not process GUI events during incomplete managed initialization; queue an
owner-bound action and let the constructor return first.

Errors are contained at the native/Lisp boundary, pause later work and are
re-signaled at the outer RUN/process-events boundary. The original error object
is preserved. Remaining queued actions stay pending for a subsequent pump if the
caller handles the error within the session; leaving WITH-APPLICATION cancels
them. An action can handle errors from its own notifying operations normally.
A nonlocal exit across the deferred callback boundary is converted to a contained
error. Return values are ignored. Cancellation is queue removal, not interruption
or rollback of an action's effects. Session finish clears pending native work
before toolkit teardown and clears all Lisp closure roots on exit.

## Example and verified scope

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/deferred-actions.lisp --eval '(wxlambda-deferred-actions:start)'
```

The example shares menu/tool/keyboard requests for file selection, copying text
and confirmed close. File selection reports the path; it does not read/write file
contents. Its automatic EXERCISE injects deterministic chooser/clipboard/confirmation
functions, avoiding interactive prompts and system clipboard changes. Separate
tests exercise real generic message/file dialogs with automatic cancellation,
modal non-reentrancy, structural operations under RUN/poll, owner expiry, FIFO,
cancellation, errors/nonlocal exits and shutdown on both Lisps. Native tests force
idle within callbacks/native operations/modal loops and transfer Unicode text
through an isolated in-memory clipboard fixture. Live automatic clipboard checks
are availability queries only; no system text is read/replaced.

Baseline: wxWidgets 3.2.12. Scheduling is bridge-specific; wxWakeUpIdle and the
managed App::ProcessIdle override supply the native wake/safe point. ProcessIdle
is present in installed native headers but omitted from the pinned interface
inventory; that inventory remains factual rather than inventing a declaration.
Physical input, OS clipboard interoperability, hosted Windows/Linux GUI, arbitrary
allocation-race injection and graphical Cadre remain unverified. This is not a
worker/thread scheduler, timer, promise/future API, transaction or shutdown hook.
See [keyboard actions](KEYBOARD-ACCELERATORS.md), [clipboard](CLIPBOARD.md),
[dialogs](MESSAGE-FILE-DIALOGS.md) and [Cadre installation](DOCSET.md).

The [persistent document form](DOCUMENT-FORM.md) now composes these APIs with
deferred shared Open/Save/Copy/Paste/close requests, toolbar/keyboard entry points
and status feedback. Its coalescing and scoped close-replay policy are application
logic; no extra public binding or native parity claim is introduced.
