# Frame menus and command actions

ABI 32 adds a bounded native menu hierarchy: MENU-RESOURCE is abstract, with
MENU-BAR, MENU and MENU-ITEM concrete classes. A frame owns one managed menu bar,
which owns its menus and items. This delivers the menus/actions part of the
selected GUI slice. [Modal text/single-choice dialogs](STANDARD-DIALOGS.md)
extend the workflow in a separate bounded interface; they are not menu resources.
The baseline is wxWidgets 3.2.12.

## Construct and route an action

```lisp
(let* ((bar (wxlambda:make-menu-bar frame))
       (file (wxlambda:make-menu bar :label "&File"))
       (open (wxlambda:make-menu-item file :id 1001 :label "&Open"))
       (option (wxlambda:make-menu-item file :id 1002 :kind :check
                                      :label "Show &details")))
  (wxlambda:bind-event frame :menu
    (lambda (snapshot)
      (format t "Action ~D checked ~S~%"
              (wxlambda:event-id snapshot)
              (wxlambda:event-checked-p snapshot)))
    :id 1001 :last-id 1002)
  (setf (wxlambda:checked-p option) t)
  (wxlambda:activate-menu-item open))
```

Generic BIND-EVENT observes :MENU command events on the frame, with existing ID
or ID-range filtering, observer ordering, per-binding :USER-DATA, immutable
snapshots and optional scoped :WITH-CONTEXT T control. Menu resources are not
event-handler receivers. Raw EVENT-INTEGER and the kind-specific checked view are
command metadata, not a resource lookup or an implicit resource-state writer.
Changing event fields does not synchronize the MENU-ITEM object. EVENT-ORIGIN-ID
is the tracked native origin when known, which can differ from the frame binding
receiver EVENT-SOURCE; it is not a portable promise about OS-generated origins. Native/user
handlers later in a dispatch can still react to those fields.

ACTIVATE-MENU-ITEM invokes native frame command processing for a managed item.
A disabled item returns NIL and does not dispatch; separators reject. The result
is the native handled boolean, not a graphical OS-menu interaction guarantee.
Native check-item activation toggles the actual resource before callbacks.
A normal-menu command can carry native integer -1, so its raw EVENT-CHECKED-P
can be true even though it is not a check item. Use CHECKED-P on a known check
resource for its actual state. Programmatic commands and actual user selection
need separate verification.

## Resources, CLOS and ownership

Direct constructors use :PARENT for the frame/bar/menu respectively.
MAKE-MENU-BAR, MAKE-MENU and MAKE-MENU-ITEM are generic thin factories with
validated :CLASS subclass support. Direct MAKE-INSTANCE, subclass slot initargs,
:DEFAULT-INITARGS and subclass AFTER hooks use the same native-backed protocol.
Native properties exist before a subclass AFTER hook runs. Abstract construction,
invalid parents and mixed managed ancestry reject. Initialization failure cleans
newly allocated resources and descendants; it does not undo arbitrary external
mutations performed by user initialization hooks.

Ownership is native frame → bar → menus → items. Lisp wrappers do not own a
second native allocation, and Lisp GC does not delete resources. Explicit
DESTROY-MENU-RESOURCE removes/deletes through the owner and invalidates all
managed descendants. Frame native destruction cleans its remaining menu tree,
including when ordinary event processing is disabled or a destroy observer does
not propagate. MENU-RESOURCE-ALIVE-P queries tracked identity within the active
session; saved wrappers do not extend native lifetime. Deleting one item leaves
its surviving siblings intact. Only one managed bar is allowed on a frame.

Construction, attachment and destruction are structural operations and reject
inside callbacks, pumps and RUN. Reading scalar properties and changing labels,
enabled flags or check state are allowed during callbacks. All native operations
require the active GUI session on its main thread. No raw native adoption,
reparenting, pop-up menus, submenus, radio groups or detach-and-reattach ownership
protocol is supplied in this slice.

## Labels, IDs and resource state

MENU-LABEL reads/writes a menu's raw bar-tab label. LABEL-TEXT reads/writes a
item's raw label, including native mnemonic/accelerator syntax;
it does not strip that syntax into display text. Separator label reads are empty,
and all separator label writes reject. Constructor labels and help
are Unicode strings without NUL. Normal/check and tab labels must be nonempty.
:HELP is construction-only. MENU-COUNT and MENU-ITEM-COUNT return native counts;
item counts include separators.

MENU-ITEM-ID and MENU-ITEM-KIND are immutable. Normal/check IDs are integers from
1 through 2147483647, unique across the managed bar. Kinds are :NORMAL (default),
:CHECK or :SEPARATOR. Separators require :ID NIL and empty label/help; use
MAKE-MENU-ITEM with :KIND :SEPARATOR. Explicit :ENABLED-P or :CHECKED-P
on a separator rejects even NIL. Normal items permit NIL checked state but reject
a non-NIL initial check. These are not arbitrary native stock-ID,
radio or existing menu-item adoption interfaces.

CHECKED-P is supported only for check menu items. Its SETF writer normalizes a
generalized boolean and returns the supplied value; reads return T/NIL.
:NOTIFY T rejects before mutation. Reads ignore :NOTIFY as in the checkbox
protocol. ENABLED-P returns the item's own native flag; its SETF returns the
supplied value and a second boolean indicating change. Separators reject both
enabled-state reads and writes. Resource scalar writes
are silent: use native command activation to exercise routing, rather than
assuming a property setter sends a command.

## Example and verification scope

[The menu example](../examples/menus.lisp) provides WXLAMBDA-MENUS:EXERCISE within an active session for
programmatic construction, property updates, command routing and cleanup:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/menus.lisp \
  --eval '(wxlambda-menus:start)'
```

START opens the managed session and runs the programmatic exercise.
CCL can use the same load/eval arguments. Final local regression, independent
review, packaging and docset evidence is recorded in the current
[handoff](../IMPLEMENTATION.md). Graphical menu selection, keyboard accelerators,
platform menu conventions and hosted Linux/Windows GUI behavior require their
own evidence. The native inventory remains partial and records additional menu
APIs without making all of them release prerequisites. Continue the representative form/editor workflow through the bounded
[standard-dialog interface](STANDARD-DIALOGS.md); message/file families still
need safe platform cancellation.

## Safe deferred structural boundary (ABI43)

[Deferred UI actions](DEFERRED-UI-ACTIONS.md) run after native callbacks and
operations return. Their outer action body is an explicit safe structural
boundary even under RUN/full polling; the quiescent structural operations in
this guide can run there. Nested event callbacks retain their existing guards,
and nested RUN/pumps/finish remain forbidden. Optional owner/cancel/error rules
are documented in that guide.

## Submenus and context choices (ABI67)

`(make-menu parent-menu :label "Recent Files")` appends an owned submenu.
`menu-label`, `menu-item-count`, item commands and recursive destruction work
at every level. IDs are unique across the entire bar. There is no detached-menu
adoption or reparenting. Structural operations use the existing deferred-action
boundaries; scalar menu state remains subject to the existing menu guards.

`(popup-choice tree '("New File…" "Rename…" "Delete…") :enabled '(t t nil))`
returns a zero-based choice or NIL on cancellation. It builds and releases a
temporary native menu, with copied labels and matching optional enable flags.
Labels are limited to1..16 choices; positions default to the native pointer
location or use client X/Y. Invoke it from a deferred action after a tree-item
menu event, never directly inside the callback. Input-operation lifetime guards
prevent receiver/ancestor destruction and suppress nested deferred work while
the popup is active. Native keyboard/mouse selection and appearance require
interactive platform verification; programmatic submenu ownership is tested.

See `examples/context-menus.lisp` for a runnable submenu and right-click example.
