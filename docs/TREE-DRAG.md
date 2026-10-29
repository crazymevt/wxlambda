# Guarded tree dragging

ABI68 adds `:tree-begin-drag` with copied `event-tree-item` and
`event-tree-position` fields. The event remains vetoed after dispatch: allowing
the built-in generic tree drag would retain raw native item pointers between
callbacks. SETF `event-vetoed-p` to NIL rejects for this event.

Capture the source item, then schedule `drag-tree-item` using `defer-ui-action`.
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

See `examples/tree-drag.lisp`. Automated tests cover native event metadata,
forced veto, callback/lifetime entry rejection and hidden-source validation.
Successful mouse drops, Escape and capture loss need interactive platform checks.
