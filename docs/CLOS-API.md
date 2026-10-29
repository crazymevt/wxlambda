# CLOS widget API

Widgets are native-backed CLOS instances. The public classes are `window`,
`grid`, `web-view`, `drawing-canvas`, `activity-indicator`, `frame`, `toolbar`, `status-bar`, `panel`, `scrolled-panel`, `notebook`, `list-box`, `tree-control`, `splitter-window`, `button`, `text-control`, `label`, `checkbox`, `choice`,
`gauge`, `slider`, `spin-control`, `radio-box`, `toggle-button`, `combo-box`, `date-picker`, `time-picker`, `search-control`, abstract `dialog`, and its `text-entry-dialog`, `single-choice-dialog`,
`message-dialog` and `file-dialog` concrete families.
`window` inherits from `event-handler` and is abstract. Standalone `event-handler`
instances are native-backed session-owned resources; window instances reuse their
existing native widget as a borrowed handler identity. See [ownership and
handler state](EVENT-HANDLERS.md). Every concrete window wrapper inherits from
`window` and represents one native widget kind. Native handles remain private
and validated.

## Construction

Use `make-instance` directly, including for your own subclasses:

```lisp
(defclass document-frame (wxlambda:frame)
  ((project :initarg :project :reader project)
   (title-changes :initform 0 :accessor title-changes))
  (:default-initargs :title "Document"))

(defmethod initialize-instance :after ((frame document-frame) &key)
  ;; Inherited initialization has already created the native widget.
  (setf (wxlambda:window-name frame) (project frame)))

(defmethod (setf wxlambda:frame-title) :after (value (frame document-frame))
  (declare (ignore value))
  (incf (title-changes frame)))

(wxlambda:with-application ()
  (let ((frame (make-instance 'document-frame :project "example")))
    (setf (wxlambda:frame-title frame) "Document — λ")
    (assert (= 1 (title-changes frame)))
    (assert (string= "example" (wxlambda:window-name frame)))
    (wxlambda:destroy-window frame)))
```

Concrete native initargs are:

- `frame`: `:title` (default "wxlambda"), `:width` (480), `:height` (240), `:accelerators` (NIL). See [keyboard actions](KEYBOARD-ACCELERATORS.md).
- `grid`: required `:parent`; rows10/columns5/editable-pT defaults, fixed bounded shape. See [grid](GRID.md).
- `web-view`: required `:parent`; initial about:blank, private WKWebView browser and asynchronous JavaScript. See [web development](WEB-VIEW.md).
- `drawing-canvas`: required `:parent`; `:content` defaults to an empty white retained scene. See [drawing](DRAWING-CANVAS.md).
- `activity-indicator`: required `:parent`; `:running-p` defaults to NIL. See [activity feedback](ACTIVITY-INDICATORS.md).
- `panel`: required `:parent` window.
- `search-control`: required `:parent`; `:value` defaults `""`, `:hint` defaults `"Search"`. Lisp WINDOW ancestry, bounded TEXT/hint properties only; see [search controls](SEARCH-CONTROLS.md).
- `time-picker`: required `:parent`; `:time` defaults to `(0 0 0)`, a copied hour/minute/second tuple. See [time-of-day values](TIME-PICKER.md).
- `date-picker`: required `:parent`; `:date` defaults to `(2000 1 1)`, a copied required Gregorian date in 1900..2100. See [calendar dates](DATE-PICKER.md).
- `slider`: required `:parent`; :minimum0/:maximum100/:value0, :orientation :horizontal.
- `spin-control`: required `:parent`; :minimum0/:maximum100/:value0. Both numeric families use strict bounds within ±1000000; see [numeric input](NUMERIC-INPUT.md).
- `toolbar`: required `:parent` frame; fixed tools after realization.
- `status-bar`: required `:parent` frame, `:fields` (1, range 1..16). See [frame actions](FRAME-ACTIONS.md).
- `button`: required `:parent`, `:label` ("Button").
- `text-control`: required `:parent`, `:value` (empty string).
- `label`: required `:parent`, `:label` (empty string).
- `checkbox`: required `:parent`, `:label` (empty string), `:checked` (NIL).
- `gauge`: required `:parent`, `:range` (100), `:value` (0), `:orientation`
  (`:horizontal` or `:vertical`). See [gauge behavior](GAUGE.md).
- `choice`: required `:parent`, `:items` (NIL), `:selection` (NIL).

Construction requires an active main-thread application session. Inherited
`initialize-instance` methods allocate exactly one native widget. Subclass
`:default-initargs`, slot initargs and ordinary `:after` hooks work normally;
these hooks can read/write native properties. An overriding primary initializer
must call its inherited method. Unsupported abstract classes, missing parents,
unknown initargs, repeated initialization and multiple native widget bases are
rejected with clear errors. Supplying a raw `:handle` is unsupported.

Failures during inherited initialization and subclass `:before`/`:after` hooks
roll back native widgets created through this protocol in that initialization,
including nested
construction, and remove their callback roots. Cleanup preserves the original
condition or nonlocal exit. Custom outer `:around` methods remain responsible
for failures they introduce after the inherited initialization has completed.

## Factory conveniences

The `make-*` functions are thin wrappers around `make-instance`. Each accepts
`:class` as a class name or class object and forwards additional initargs:

```lisp
(wxlambda:make-frame :class 'document-frame :project "example")
(wxlambda:make-button parent :class 'wxlambda:button :label "Run")
```

A factory validates that `:class` is a subclass of its corresponding concrete
widget class. Child factories dispatch on their parent; methods extending a
factory should accept `&rest`/`&allow-other-keys` to forward subclass initargs.
`make-frame` remains an ordinary function because it has no required dispatch
argument. Application lifecycle functions/macros and immutable event snapshots
keep their separate roles.

## Generic properties

Use these thirteen generic `setf` accessors:

```lisp
(setf (wxlambda:text input) "Text")
(setf (wxlambda:label-text caption) "Ready")
(setf (wxlambda:checked-p checkbox) t)
(setf (wxlambda:selection choice) 0)
(setf (wxlambda:window-name window) "logical-name")
(setf (wxlambda:frame-title frame) "Title")
(setf (wxlambda:shown-p window) nil)
(setf (wxlambda:enabled-p window) t)
(setf (wxlambda:gauge-value gauge) 25)
(setf (wxlambda:gauge-range gauge) 100)
(setf (wxlambda:handler-enabled-p handler) t)
(setf (wxlambda:event-skipped-p context) t) ; only in its contextual callback
(setf (wxlambda:event-vetoed-p context) t)  ; only in a vetoable close context
```

A writer's method lambda list starts with the new value, followed by the
receiver. Specialize `(setf accessor)` with primary, `:around`, `:before` or
`:after` methods; `call-next-method` retains inherited behavior. Assignment
returns the supplied value. Boolean reads normalize generalized booleans to
T/NIL. The shown/enabled writers also return a native changed-state flag as a
second value; `show-window`/`enable-window` are action conveniences returning
the window and that flag.

Text, checkbox and selection writes are silent by default. Use `:notify t`
on the place for synchronous command events:

```lisp
(setf (wxlambda:text input :notify t) "Changed")
(setf (wxlambda:checked-p checkbox :notify t) t)
(incf (wxlambda:selection choice :notify t))
```

The corresponding readers accept and ignore `:notify`, so modifying macros
can read the same place. Callback conditions are re-signaled at the writer's
Lisp boundary. Reads of `shown-p` use the own shown flag; reads of `enabled-p`
include disabled ancestors, while its writer requests the own enabled state.

This project is establishing its API from the ground up: the six separate
`set-*` property aliases and compatibility-only wrong-receiver methods have
been removed. Unsupported receiver types signal standard Lisp dispatch errors;
native handle/state failures still signal `native-error`.

## Scope and verification

The [metadata example](../examples/window-metadata.lisp) uses direct subclass
construction and a generic writer hook. Lisp subclassing extends the wrapper;
it does not change the native C++ class or provide native virtual callbacks.
Native ownership, main-thread restrictions and explicit handler unbinding before
destruction still apply. Start a fresh Lisp when changing these class definitions.

```sh
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/clos.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/clos.lisp
```

The CLOS test covers direct native construction for the original seven concrete types, subclass
slots/defaults/hooks, all factory `:class` paths, invalid construction, initializer
rollback of nested native objects and callback roots, eight generic writers,
method combination and native handle protection. Those checks exercise the original 34 named generic functions and eight generic
SETF functions. The docset includes
construction instructions on class pages and indexed SETF forms on property pages.

The gauge scenario additionally verifies direct and factory subclass construction,
subclass AFTER access, rollback, and the two gauge SETF writers. Across the public
API there are now 55 named generic functions and thirteen generic SETF functions.

Independent `event-binding` values are immutable registration identities rather
than native-backed CLOS resources. The generic `bind-event`, `unbind-event`,
`binding-active-p` and `queue-event` operations cover the four current command
kinds; see [binding scope and lifecycle](EVENT-BINDINGS.md).

`event-context` is a borrowed, immutable-token view whose generic operations
mutate a native event only during its current callback. Its scoped SETF writer
does not change immutable snapshot semantics. See [context lifetime](EVENT-CONTEXTS.md).

Close capability/veto readers accept immutable snapshots or current contexts;
the veto SETF writer accepts only a current vetoable close context. Frame close
requests use the generic `close-window`; see [close-family scope](CLOSE-EVENTS.md).

`process-event` synchronously dispatches supported copied events and returns
native handled/final-veto values. Registered custom families remain session-scoped
metadata; see [notify type scope](NOTIFY-EVENTS.md).

See the [borrowed application-handler view](APPLICATION-HANDLER.md) for managed wxApp identity, inherited dispatch and lifetime restrictions. Session bindings and logging-off metadata are available; real OS delivery remains unverified.

[Session events](SESSION-EVENTS.md) extend the close family with query/end-session kinds, scoped logging-off metadata and isolated synthetic dispatch. Native OS session delivery remains unverified.

[Owned events](OWNED-EVENTS.md) now provide detached native construction/cloning,
explicit disposal, mutable value fields and copied dispatch. Earlier immutable
snapshots and scoped contexts keep their lifetime rules. Arbitrary native classes,
origin retention and client/userData ownership remain open.

[Standalone handler chains](HANDLER-CHAINS.md) now preserve canonical wrapper
identity and session ownership while participating in normal/local/pending
delivery. Structural edits and linked destruction during callbacks reject;
borrowed window/application stacks remain unbound.

[Managed window handler stacks](WINDOW-HANDLER-STACKS.md) now attach owned
standalone handlers without transferring native ownership, detach them on actual
window destruction and provide explicit stack-aware copy processing. Standalone
peer editing excludes attached nodes; arbitrary SetEventHandler/external adoption
remain unbound.

[Handler Lisp client data](HANDLER-CLIENT-DATA.md) now retains EQ values through
a native typed token adapter with safe foreign-mode rejection. General native event/client/item pointer ownership remains unbound; per-binding
Lisp USER-DATA is separately supported.

## Native-owned menu resource classes

MENU-RESOURCE is abstract; MENU-BAR, MENU and MENU-ITEM are native-backed resource
classes outside the window/event-handler hierarchy. Direct MAKE-INSTANCE uses
:PARENT frame/bar/menu. Generic MAKE-MENU-BAR/MAKE-MENU/MAKE-MENU-ITEM accept
validated :CLASS and subclass initargs, including DEFAULT-INITARGS. Native
properties exist in subclass AFTER hooks. Mixed managed ancestry rejects before
allocation; failed initialization removes its new native resource/descendants.
This cleanup does not reverse arbitrary edits to existing external resources.

Native frame→bar→menu→items ownership controls lifetime. Explicit destruction or
parent destruction invalidates descendant identities; wrappers and GC do not
retain/delete native objects. Structural operations reject callbacks/pumps/RUN,
while scalar label/check/enabled writes are allowed. See [menus](MENUS.md) for
separator constraints, raw labels, per-bar IDs and command routing.

## Modal dialog families

DIALOG is an abstract WINDOW subclass. TEXT-ENTRY-DIALOG and SINGLE-CHOICE-DIALOG
use native-backed direct initialization with optional :PARENT plus message/title
and family payloads. Their keyword-only MAKE-* factories remain ordinary functions
with validated :CLASS forwarding. Native properties are ready in subclass AFTER
hooks; failure cleans new resources and roots. The modal-only protocol and
active-modal/ancestor destruction guards are documented in
[standard dialogs](STANDARD-DIALOGS.md). No native virtual subclass or general
modeless/validation API is implied.

Generic MESSAGE-DIALOG and FILE-DIALOG extend the same DIALOG hierarchy with
keyword-only ordinary factories and Construction entries. Their fixed generic
wxWidgets backend preserves eager native properties for subclass AFTER hooks,
explicit parent normalization and managed invalidation. Native OS dialog
implementations remain unbound. See [message/file workflows](MESSAGE-FILE-DIALOGS.md).

The singleton `clipboard-text` property uses a zero-argument generic reader and
a generic SETF writer specialized on STRING, without a public clipboard class.
See [text clipboard](CLIPBOARD.md) for transfer, errors and persistence scope.

Text controls accept `:multiline` (default NIL) and `:editable-p` (default T).
Native properties are available in subclass AFTER hooks; editable/selection/caret
properties have generic SETF. See [text editing](TEXT-EDITING.md) for native offset
semantics, notifying replacements, read-only policy and limits.

The concrete `notebook` window uses native-backed CLOS construction and fixed
fully initialized `panel` pages. Page identity is rooted before native publication;
page self-attachment during initialization rejects. See [notebook navigation](NOTEBOOK.md).

`list-box` constructs an unsorted native single-selection list from `:items` and
`:selection`; subclass AFTER hooks can query/edit it. Item text uses generic SETF.
See [single-selection lists](LIST-BOX.md) for item and callback lifetime rules.

`tree-control` is a concrete native-backed window. `tree-item` is a borrowed checked
view, obtained from tree APIs rather than direct MAKE-INSTANCE. Canonical view
identity can outlive native validity; see [tree ownership/navigation](TREE-NAVIGATION.md).

## Fixed splitter panes

The bounded splitter batch adds native-backed `splitter-window`, persistent panel
panes and guarded sash notifications. See [supported scope and example](SPLITTER.md).
Dynamic pane replacement/removal and complete splitter parity remain deferred.

## Frame toolbar/status actions

Native-backed `toolbar` and `status-bar` add a bounded application-shell slice:
fixed normal/check/separator tools with stock art and :menu action routing, silent
tool state and application-owned indexed status fields. See
[supported scope, backend policy and example](FRAME-ACTIONS.md). Native Cocoa
toolbar chrome, dynamic tools and status widths/stacks remain deferred.

ABI43 [deferred UI actions](DEFERRED-UI-ACTIONS.md) provide safe FIFO work after
callbacks, including modal/clipboard/destructive requests. Existing nested event
callback guards still apply; pending-only dispatch does not run the action queue.

ABI44 UI-TIMER is an immutable scheduler identity obtained from START-UI-TIMER,
like UI-ACTION for deferred work; it is not a native event-handler/resource wrapper.
No public wxTimer subclass/Notify override/adoption protocol is exposed. See
[UI timers](UI-TIMERS.md).

RADIO-BOX adds fixed mutually exclusive choices via :parent/:items/:label/:selection/:orientation; see [radio groups](RADIO-GROUPS.md).

TOGGLE-BUTTON supports :parent/:label/:checked with direct subclass slots/defaults and native-ready AFTER hooks. See [toggle buttons](TOGGLE-BUTTONS.md).

COMBO-BOX adds :parent/:items/:value/:selection with native-backed subclass construction, properties in AFTER hooks and managed rollback. See [combo boxes](COMBO-BOXES.md).

SCROLLED-PANEL is a WINDOW subtype backed by wxScrolled<wxPanel>, with :parent/:scroll-rate and native-ready subclass hooks. Nest it inside ordinary notebook/splitter PANEL content. See [scrolling panels](SCROLLING-PANELS.md).

DATE-PICKER is a WINDOW subtype with :parent/:date, a copied Gregorian tuple and native-ready subclass hooks. See [required calendar dates](DATE-PICKER.md).

TIME-PICKER is a WINDOW subtype with :parent/:time, a copied time-of-day tuple and native-ready subclass hooks. See [time pickers](TIME-PICKER.md).

SEARCH-CONTROL is a WINDOW subtype with :parent/:value/:hint, copied query and placeholder strings, and native-ready subclass hooks; it does not inherit Lisp TEXT-CONTROL. See [search controls](SEARCH-CONTROLS.md).
