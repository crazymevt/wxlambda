# Guarded tree dragging

ABI69 corrects native startup for `:tree-begin-drag` with copied `event-tree-item` and
`event-tree-position` fields. The event remains vetoed after dispatch: allowing
the built-in generic tree drag would retain raw native item pointers between
callbacks. SETF `event-vetoed-p` to NIL rejects for this event.

Call `drag-tree-item` synchronously inside the matching `:tree-begin-drag`
callback. Cocoa needs the original native mouse event to remain live: deferring
the drag itself to idle prevents it from starting. Defer only application/tree
mutations until the callback returns.
The operation installs a temporary private same-tree target and runs a native
move drag. It returns a target item or NIL on cancellation/rejection; native
errors signal. It never mutates tree items or files. Validate source and target
liveness and apply application policy after return. MSW requires an associated tree image list to initiate BEGIN (the example
installs one). The source tree must be
shown/enabled and have no existing drop target. There is no cross-tree transfer,
external file drop, built-in drag Allow, right-button drag or copy operation.

Input and tree operation guards span the nested drag loop and target teardown;
structural mutations, receiver/ancestor destruction and nested native workflows
reject, while deferred actions and shared timers wait. Each drag has a unique
private token. No persistent BEGIN/END latch survives Escape or capture loss.

See `examples/tree-drag.lisp`. Automated tests cover native event metadata, forced veto, exact source/callback
matching, synchronous entry, success/cancellation results and nested-operation
guards. A test driver exercises the guarded drag boundary without OS mouse input.
Successful mouse drops, Escape and capture loss need interactive platform checks.
