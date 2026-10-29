# Search controls and Lisp-native filtering

ABI54 adds native-backed `search-control`, generic `make-search-control`, generic
`search-hint`/SETF and TEXT/SETF methods for the new receiver. The control is an
editable single-line query field with native search/cancel gestures. Filtering
is application policy; the widget does not search a collection on its own.

```lisp
(defparameter query
  (wxlambda:make-search-control parent :value "λ" :hint "Find records"))
(wxlambda:text query) ; fresh native query string
(setf (wxlambda:text query) "notes") ; silent
(setf (wxlambda:text query :notify t) "notes") ; one TEXT event
(setf (wxlambda:search-hint query) "") ; silent placeholder update
```

`:parent` is a required WINDOW, `:value` defaults to `""` and `:hint` to the
literal `"Search"`. Both fields accept copied UTF-8 strings with explicit byte
lengths and reject NUL/CR/LF. Empty query and hint strings are valid. Reads are
fresh native strings, not retained input storage; SETF returns the supplied
string. Native readback, caret/selection changes and presentation remain native.
There is no multiline/read-only/style/menu/raw-adoption API for this slice.

SEARCH-CONTROL inherits Lisp WINDOW, not TEXT-CONTROL, because wxSearchCtrl is
not a wxTextCtrl subclass on every native port. Only the bounded TEXT property
and search-specific hint property are added: text editing/selection/insertion
APIs and ON-TEXT-CHANGE convenience methods retain their existing receiver scope.
Direct MAKE-INSTANCE subclasses support slot initargs/defaults and native-ready
AFTER hooks, with native/root rollback if initialization fails. The generic
factory validates `:class` and forwards subclass initargs. Lisp subclasses do
not add native C++ virtual overrides. See [CLOS API](CLOS-API.md).

## Native gestures and explicit application policy

Use BIND-EVENT with existing `:text-change` plus new native-only `:search` and
`:search-cancel` command kinds:

```lisp
(wxlambda:bind-event query :search
  (lambda (event)
    (format t "Native search payload: ~S~%" (wxlambda:event-text event))))
```

Search/cancel snapshots copy native GetString, including empty strings. They do
not infer the query from the widget or promise that payload and live query match.
Observers automatically Skip; contextual callbacks have ordinary explicit Skip/
propagation behavior. Context string metadata may be written without editing the
query; snapshots remain immutable. Source-bound TEXT event string metadata writes
reject because native TEXT payloads may lazily read the editor. This rejection also
applies to generic search ports lacking the native text-entry hook, giving this
new receiver one portable metadata-write rule. Generic TEXT
owned/synthetic/cloning support retains its established scope; new search/cancel
kinds reject owned/synthetic construction and detached clones.

Programmatic TEXT writes are silent by default; `:notify t` emits exactly one
native TEXT event even unchanged, after ChangeValue. It does not emit search or
cancel. Hint writes are silent and have no notify option. Original callback
conditions re-signal after the native setter returns with committed text retained.
There is no public synthetic submit/cancel operation in this slice; applications
can call their own filtering function independently of native gestures.

The bridge requests both native icons and wxTE_PROCESS_ENTER at construction.
Their actual visibility and gestures remain port policy: GTK SearchEntry shows
search permanently and cancel automatically when nonempty; its search icon is
inactive and does not submit. Generic/GTK Enter submission normally suppresses
empty queries. Cocoa may interpret an empty action as cancel. Cancellation can
clear native text before or during dispatch, and event text may be empty.
Native TEXT/search/cancel count/order and cancel default effects are not
normalized by the bridge. Consuming an event does not guarantee restoring text
already changed natively. No button visibility query/write protocol is exposed.

Actual search/cancel/TEXT origins, propagated callbacks and composite-child TEXT
use shared Input guards: same-control query/hint writes, source/ancestor destroy/
close and nested GUI/RUN/pumps, modal/clipboard entry and safe-idle delivery reject.
Silent cross-search writes remain supported. Creation, early managed Destroy and
native-base teardown suppress relevant input callbacks; surviving native refusal/
exception restores the guard state. Saved snapshots outlive callback/session;
borrowed contexts obey current-innermost lifetime/expiry.

Use owner-bound deferred actions to implement cancel or filtering after native
processing. For example, explicit clearing avoids assuming a portable cancel
side effect:

```lisp
(wxlambda:bind-event query :search-cancel
  (lambda (event)
    (declare (ignore event))
    (wxlambda:defer-ui-action
      (lambda ()
        (setf (wxlambda:text query) "")
        (filter-records)) :owner query)))
```

The [example](../examples/search-controls.lisp) filters a Lisp list into LIST-BOX
items using CHAR-EQUAL substring matching, not locale/Unicode search collation.
TEXT/search callbacks defer filtering and read the query at delivery time; cancel
and the Clear filter button defer an explicit clear plus filtering. Owner removal
cancels the pending action. This is an example policy, not native search behavior.
See [deferred actions](DEFERRED-UI-ACTIONS.md).

## Build and verification scope

CMake requires wxUSE_SEARCHCTRL=1 and wxUSE_TEXTCTRL=1; native linkage remains
core/base/adv (adv is already required by date/time controls). Tests cover copied
Unicode fields, validation, silence/explicit TEXT notify, CLOS rollback, native
search/cancel/TEXT events outside setters, propagation, composite input/teardown,
TEXT clone preservation and callback ABI string copies. The Lisp callback-copy
probe is a fabricated ABI payload, not a native gesture simulation.

Physical typing/IME/Enter/search/cancel input, locale/collation/accessibility,
port icon presentation and hosted Windows/Linux/Qt GUI behavior remain unverified,
as does graphical Cadre. Native event/early teardown probes run on macOS;
source audits describe generic/GTK differences without claiming hosted tests.
Tests never read or overwrite system clipboard contents. See
[Cadre installation](DOCSET.md), [text editing](TEXT-EDITING.md) and
[native search reference](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/srchctrl.h).
