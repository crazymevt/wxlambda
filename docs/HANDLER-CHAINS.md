# Managed standalone event-handler chains

ABI 31 adds NEXT-HANDLER, PREVIOUS-HANDLER, HANDLER-UNLINKED-P and
UNLINK-EVENT-HANDLER for session-owned standalone EVENT-HANDLER objects and
subclasses. The two peer properties have generic SETF writers. The baseline is
wxWidgets 3.2.12. Borrowed window/application sources and peers reject these
operations; dedicated managed [window stack APIs](WINDOW-HANDLER-STACKS.md) now cover
Push/Pop/Remove while preserving bridge ownership. General standalone peer
operations still exclude attached/borrowed window chains.

## Canonical peers and native ownership

NEXT-HANDLER and PREVIOUS-HANDLER return the existing managed Lisp peer, or NIL
for a vacant endpoint. Repeated lookup is EQ to the original wrapper, including
subclasses and their slot state. These are wrapper references, not native pointer
values or reconstructed objects. HANDLER-UNLINKED-P is true only when both peer
links are NIL.

The session strongly roots standalone wrappers in a canonical identity map until
explicit destruction, failed-construction rollback or session cleanup. This is
true even for unlinked handlers: dropping application references does not make a
new wrapper appear on lookup or destroy native state. Native ownership remains
in the session registry; linking does not adopt, transfer or parent a handler.
There is no GC finalizer. Native fields remain available in subclass AFTER hooks
and constructor rollback removes canonical registrations and native resources.
It does not undo arbitrary edits made to pre-existing external chains. Removing
a failed node invokes native Unlink, which can leave surviving prefix/suffix
segments joined. Resource cleanup is not a graph-state transaction.

Reads require the active main-thread session and valid non-deleting standalone
handles, but are permitted during callbacks. Outside-session and stale/pending
native access signal NATIVE-ERROR. Window/application receivers and peers signal
NATIVE-ERROR before mutation rather than exposing a borrowed native chain.

## Reciprocal linking and splitting

The Lisp SETF protocol preserves both directions of each edge. It is deliberately
stronger than calling one raw native setter and leaving the other link stale.

```lisp
(wxlambda:with-application ()
  (let ((first (wxlambda:make-event-handler))
        (middle (wxlambda:make-event-handler))
        (last (wxlambda:make-event-handler)))
    (unwind-protect
         (progn
           (setf (wxlambda:next-handler first) middle
                 (wxlambda:next-handler middle) last)
           (assert (eq first (wxlambda:previous-handler middle)))
           (wxlambda:unlink-event-handler middle)
           (assert (eq last (wxlambda:next-handler first)))
           (assert (wxlambda:handler-unlinked-p middle)))
      (dolist (handler (list first middle last))
        (wxlambda:destroy-event-handler handler)))))
```

Setting NEXT-HANDLER joins the source's vacant next endpoint to the peer's vacant
previous endpoint; setting PREVIOUS-HANDLER is the mirror operation. Existing
segments can be joined at compatible vacant endpoints. Repeating an already
reciprocal pair is idempotent. Self-links, cycles, occupied conflicting endpoints
and invalid peers reject before changing either chain. SETF returns the supplied
peer wrapper.

The pinned interface text says native SetNextHandler/SetPreviousHandler cannot
receive NULL, but the implementation setters permit pointer clearing and native
Unlink uses the same paired clearing pattern. Lisp NIL is a bounded reciprocal
detach operation, not an exposed one-sided null setter. Retain this interface/
implementation discrepancy for C7 reconciliation.

SETF NIL detaches just that edge and its reciprocal link, splitting a chain into
two segments; it returns NIL. It does not remove the source from both neighbors.
UNLINK-EVENT-HANDLER instead splices the source's previous and next neighbors
together and leaves the source detached and alive, returning the source handler.
Unlinking an already detached handler is harmless.

All mutations require a non-dispatch boundary: RUN, native pumps and callbacks
reject linking, edge detachment and UNLINK, even if the requested change would
be idempotent. Read peers during a callback and schedule structural changes for
after dispatch instead. No new asynchronous mutation scheduler is supplied. Attached pushed nodes also
reject general peer reads/SETF/UNLINK; HANDLER-UNLINKED-P can report false. Use the
window stack APIs to inspect or detach those nodes.

## Dispatch and lifetime

Normal processing, native pending delivery and PROCESS-EVENT-LOCALLY now traverse
these standalone native chains using wxWidgets rules. An observer Skips, so a
following node can run. A consuming contextual callback can stop local chain
handling. Disabling a node's HANDLER-ENABLED-P skips its own handling without
disabling following nodes. Skip, parent propagation and local chain traversal
remain distinct native mechanisms; STOP-EVENT-PROPAGATION is not a promised
replacement for consuming a chain event.

Each snapshot's EVENT-SOURCE is the wrapper on which that callback was registered.
It can be a downstream handler while EVENT-ORIGIN-ID still identifies the original
delivery receiver. Callback modifications remain subject to native matching and
snapshot immutability; native node traversal follows the next-link chain, but linking does not promise
callback registration order within an individual node.

Destroying a linked handler during a callback rejects before native deletion so
the active chain is not edited under native traversal. Destroying an unlinked
handler from a callback keeps the existing deferred-destruction behavior. Outside
callbacks, native handler destruction unlinks/splices a linked node; its neighbors
remain alive and reciprocal. Explicit UNLINK never destroys the node. Session
cleanup removes bindings, canonical roots and remaining native handlers safely.

Pending queues belong to their target handler, not to every node that might
later observe an event through a chain. DELETE-PENDING-EVENTS deletes only its
receiver's own queue. It does not cancel upstream queued events merely because
they might later traverse that node. Linking/unlinking does not move queued
objects between queues. Destination destruction and consuming TRANSFER-EVENT
retain their existing ownership rules.

Session tags remain isolated from wxApp fallback. Managed chains cannot include
a borrowed application/window handler, and standalone session fallback remains
blocked. Native chain support does not authorize synthetic OS session delivery,
window pushed-handler ownership or arbitrary native chain edits.

Run the example:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/handler-chains.lisp --eval '(wxlambda-handler-chains:start)'
```

Validation should cover reciprocal/canonical peers, segment joining/splitting,
cycle/occupied-endpoint rejection without mutation, unlink/destructor splicing,
Skip/consumption/disabled-node delivery, pending/local dispatch, callback mutation
rejection and constructor rollback. Local results do not establish hosted
Windows/Linux GUI, real OS-session or graphical Cadre validation.

Coverage remains partial: borrowed window/app chains, native Push/Pop/Remove,
external/native handler adoption, worker access, ownership transfer, native
virtual overrides and arbitrary event-origin retention remain required C1 work.
Next define borrowed window handler-stack ownership before exposing Push/Pop or
widening chain receivers; preserve managed reciprocal identity and dispatch guards.

Build/check with `python3 tools/docset.py build` and `check`; install with
`python3 tools/docset.py install` under `$XDG_DATA_HOME/docsets/wxlambda/`
(default `~/.local/share/docsets/wxlambda/`) for Cadre discovery and lookup.
See [handler lifetime](EVENT-HANDLERS.md), [pending delivery](PENDING-EVENTS.md),
[bindings](EVENT-BINDINGS.md) and [docset installation](DOCSET.md).

[Managed window stacks](WINDOW-HANDLER-STACKS.md) now provide bounded Push/Pop/
Remove and explicit stack-aware synchronous processing; external adoption and
general SetEventHandler remain gaps.

[Handler Lisp client data](HANDLER-CLIENT-DATA.md) now retains EQ values through
a native typed token adapter with safe foreign-mode rejection. General native event/client/item pointer ownership remains unbound; per-binding
Lisp USER-DATA is separately supported.
