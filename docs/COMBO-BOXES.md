# Editable combo boxes with fixed choices

ABI48 adds native-backed `combo-box`, an editable single-line WINDOW subclass
with fixed unsorted choices. Required `:parent` is a window. `:items` defaults
to NIL and accepts a finite proper list of0..256 nonempty strings. Native
wxString::CmpNoCase must distinguish every pair; duplicate/case-only choices
reject to avoid known MSW ambiguities. This is native comparison, not Unicode
normalization or full casefold identity. Choice/value strings reject NUL, CR and
LF. Unicode single-line values are supported. Baseline wxWidgets3.2.12; build
requirement wxUSE_COMBOBOX.

`:value` defaults to empty text. Optional `:selection` is NIL or a valid zero-based
choice index. A non-NIL selection cannot accompany nonempty initial value;
it initializes text to the selected item. Empty choice lists still accept arbitrary
editor text. Native selection readback after arbitrary text is platform-dependent.
There is no read-only, sorted, popup/style, owner-drawn or bitmap variant here.

```lisp
(let ((destination (make-instance 'wxlambda:combo-box :parent panel
                                  :items '("Inbox" "Archive") :selection 0)))
  (setf (wxlambda:selection destination :notify t) 1)
  (setf (wxlambda:text destination :notify t) "Custom λ")
  (setf (wxlambda:selection destination) nil)) ; clear both selection and text
```

`make-combo-box` is a thin generic factory with validated `:class` and forwarded
subclass initargs. Direct subclasses support slots/default initargs and native
properties in AFTER hooks; invalid mixed native ancestry rejects and managed
initialization failure cleans widgets/callback roots. See [CLOS API](CLOS-API.md).

## Text, selection and fixed item properties

Generic `text`/SETF reads/writes independent explicit-length UTF-8 strings.
A text write clears the prior native list selection then calls silent ChangeValue;
native backends may recognize matching text again. Read `selection` if needed;
text is not a promise that an item was chosen. Native typing/popup matching also
varies by port. Single-line validation applies to programmatic writes; physical
editing uses native platform behavior.

Generic `selection` reads native committed GetSelection, returning index/NIL.
It does not promise the currently highlighted open-popup item (notably MSW).
SETF requires an existing index or NIL. An index write sets the item's text and
selection under blockers; NIL clears selection and text. Both readers ignore
their shared `:notify` keyword. Writes return the supplied value and are silent
by default. `:notify t` on TEXT emits exactly one :text-change; on SELECTION emits
exactly one :combo-selection, even unchanged or NIL. Selection notify does not
also emit text notify. Callback errors re-signal original conditions after the
native call without rolling committed state back.

`item-count` returns the fixed count; `item-text` copies native text at a checked
index. There are no append/delete/clear/item-text writer methods for COMBO-BOX.
Text selection/caret/editability/clipboard editing APIs remain TEXT-CONTROL-only;
no ON-SELECTION/ON-TEXT-CHANGE convenience methods are added. Use generic bindings.

## Events and native callback safety

Bind `:combo-selection` for chosen-list updates and existing `:text-change` for
editor updates. Both are native command events carrying text. Combo selection
also exposes copied/current `event-selection`; unrelated TEXT snapshots have NIL
selection metadata. Default observers Skip automatically; frame observers can
receive propagation. Physical selection may emit COMBO then TEXT, with native
order/count varying by backend. Exact-one guarantees apply only to explicit
bridge setter notification, not to physical input.

Immutable snapshots survive source/session cleanup. Borrowed contexts require
the innermost current callback and expire on return. Semantic EVENT-SELECTION
SETF, owned/synthetic construction and detached clones reject for the new combo
selection kind. Existing generic TEXT event construction/dispatch rules retain
their scope; the new restriction is not a blanket ban on all combo-origin events.
Raw current integer edits and combo-selection text edits are metadata, not
calls to the combo setter. Source-bound native TEXT context text SETF rejects
because text-entry payloads can be lazy; existing owned TEXT dispatch also
retains its text-entry receiver restrictions.

Shared Input guards cover actual native COMBO and TEXT origins, composite child
identities and parent propagation. Same-combo text/selection writes reject during
callbacks/operations, source/ancestor destruction/close defer, and modal/clipboard/
RUN/pump/nested safe-idle entry reject. Silent updates of another combo are allowed.
Creation/failure/base teardown suppress relevant callbacks. Matching uses tracked
live identities without unchecked event-object casts/dereferences. Constructor
and programmatic writes block incidental native callbacks before explicit notify.
No native pointer adoption or full wxComboBox/wxTextEntry/item-container parity.

## Example and verification

[examples/combo-boxes.lisp](../examples/combo-boxes.lisp) selects a destination
preset or accepts a custom value. START is interactive. EXERCISE verifies silent
selection, one combo notification, one text notification, UTF-8 and NIL clear.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/combo-boxes.lisp --eval '(wxlambda-combo-boxes:start)'
sbcl --noinform --non-interactive --load ci/setup.lisp --load tests/combo-boxes.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/combo-boxes.lisp
```

Tests cover construction/subclasses/rollback, fixed items and validation, text vs
selection, snapshots/current contexts, callback error identity/committed state
and stale handles. Native probes observe constructor silence and dispatch COMBO
then TEXT directly outside setter guards, including parent propagation, protected
source/ancestor/mutation/modal/clipboard/loop entry and silent cross-combo writes.
Runtime evidence is macOS wxWidgets3.2.12; OSX/GTK/MSW/Qt paths were source-reviewed.
Physical popup/typing/IME/key/accessibility input, hosted Linux/Windows/Qt GUI,
arbitrary native allocation failures and graphical Cadre remain unverified.
Automatic tests do not read/replace system clipboard contents. See
[coverage](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).
