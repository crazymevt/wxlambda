# Bounded radio-button groups

ABI46 adds `radio-box`, a native-backed wxRadioBox WINDOW subclass containing a
fixed group of mutually exclusive choices. This composite owns its native radio
buttons; grouping does not depend on sibling creation order. Required `:parent`
is a window, `:items` is a proper list of2..64 strings, `:label` defaults to an
empty group caption, `:selection` defaults to0, and `:orientation` defaults to
`:vertical`. Horizontal uses one row, vertical one column. Selection is always
a zero-based valid index, never NIL. Strings reject embedded NUL; duplicate or
empty labels are allowed. Native mnemonic/display normalization (notably `&`)
can differ by port; native item/event text need not equal supplied strings.
Baseline wxWidgets3.2.12; builds require wxUSE_RADIOBOX/RADIOBTN/STATBOX.

```lisp
(let ((group (make-instance 'wxlambda:radio-box :parent panel
                           :label "Format" :items '("Text" "HTML" "Markdown"))))
  (setf (wxlambda:selection group) 1)             ; silent
  (setf (wxlambda:selection group :notify t) 2)   ; one notification
  (wxlambda:item-text group (wxlambda:selection group)))
```

`make-radio-box` is a thin generic factory with validated `:class` and forwarded
subclass initargs. Direct subclasses support slots/default initargs and native
properties in AFTER hooks. Invalid mixed native ancestry rejects before native
allocation; failed managed initialization releases widgets and callback roots.
See [CLOS construction](CLOS-API.md).

## Selection and fixed items

`selection` and its generic SETF specialize on RADIO-BOX. Values outside the
fixed list reject without mutation. The reader ignores the shared `:notify`
keyword. Writes are silent by default. `:notify t` commits then emits exactly one
native-class wxCommandEvent, even when the index is unchanged; callback errors
are re-signaled at the Lisp boundary without rolling selection back. Physical
repeated-selection events can differ by port. `item-count` returns the fixed
count; `item-text` returns native text at a checked index. No item insertion,
removal, rename, group caption setter or runtime orientation change is exposed.
Existing LIST-BOX-only item mutation methods do not apply to RADIO-BOX.

## Events and callback safety

Use `bind-event` with `:radio-selection`, optionally `:with-context t`.
`event-selection` reads the index from an immutable snapshot or the innermost
current borrowed context. `event-text` reads copied/native label text;
`event-integer` is the raw command payload. Snapshot values remain readable after
source/session cleanup. Contexts expire on return. Events propagate as native
command events, including frame observers; default observers Skip automatically.
There is no dedicated on/off-selection convenience method for this class.

Numeric and radio composites share input lifetime guards. Actual source events
and frame-propagated events protect the group and ancestors from destruction/
close and defer modal/clipboard transfers, RUN/pumps and nested safe-idle entry.
Same-group selection writes reject during its callback/operation; silent writes
to another group are allowed. Creation/failure/teardown suppress relevant input
callbacks. Origin matching uses tracked live identities/composite descendants,
without unchecked casting or dereferencing event-object pointers. Native setter
calls are blocked from incidental delivery; explicit notify runs after blockers.

Radio events cannot be constructed as owned/synthetic events or detached/cloned.
EVENT-SELECTION SETF is not supported for this kind. Raw current event integer/
text metadata changes are observational and do not modify the control or confer
veto behavior. This is wxRadioBox support, not standalone wxRadioButton grouping,
per-item enable/show/focus/tooltips, arbitrary styles or native class completeness.

## Example and verification

[examples/radio-groups.lisp](../examples/radio-groups.lisp) selects an export format
and updates a label. START is interactive; EXERCISE tests silent/notifying
selection and cleanup without hardware input.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/radio-groups.lisp --eval '(wxlambda-radio-groups:start)'
# In an initialized project environment:
sbcl --noinform --non-interactive --load ci/setup.lisp --load tests/radio-groups.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/radio-groups.lisp
```

Tests cover direct/subclass/factory construction, UTF-8, bounds, rollback, callback
source/ancestor protection, silent cross-group updates, event lifetime and clone
refusal. Native probes dispatch actual wxEVT_RADIOBOX command events outside the
setter operation, including parent propagation; these do not simulate mouse or
keyboard input. Runtime evidence is macOS3.2.12; OSX/GTK/MSW/Qt implementations
were source-reviewed. Hosted Linux/Windows/Qt GUI, physical clicks/arrows,
accessibility and graphical Cadre remain unverified. Automatic clipboard tests
do not read or replace system contents. See [coverage](WXWIDGETS-COVERAGE.md) and
[Cadre installation](DOCSET.md).
