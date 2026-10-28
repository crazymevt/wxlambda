# Multiline text and basic editing

ABI36 extends `text-control` with multiline construction, user editability and
native text offsets. Existing calls still create a single-line editable control:

```lisp
(wxlambda:make-text parent :value "Name")
(make-instance 'wxlambda:text-control :parent parent
               :value (format nil "First λ~%Second 日本語 😀")
               :multiline t :editable-p nil)
```

`:multiline` defaults NIL and fixes the native multiline style at construction;
`text-multiline-p` reads it. `:editable-p` defaults T; generic SETF
`text-editable-p` changes whether the user can type. Like wxWidgets, read-only is
user-input policy: explicit Lisp `(setf text)` and `replace-text` still change its
contents. Lisp subclass slots, `:default-initargs` and native access from subclass
AFTER hooks work with both options. Failed managed initialization destroys native
controls and releases callback roots. The factory remains a thin validated
`:class` wrapper, with no separate multiline class or compatibility aliases.

## Values and notifications

`text` returns an independent Lisp string; SETF returns the supplied string.
Constructor, setter and range replacement validate strings and reject embedded NUL,
rather than silently truncating through a C string. Native UTF-8 interfaces carry
explicit byte lengths. Multiline values use wxWidgets' LF-separated text format,
including on Windows. Single-line newline handling remains native-port behavior.
No styles for password, rich text, wrapping, tabs, Enter processing or input filters
are added. Rich-text/STC controls and native file load/save are outside this slice.

Whole-value SETF is silent by default; `:notify t` uses native SetValue instead of
ChangeValue. `replace-text` uses native Replace and its text-change notification:

```lisp
(wxlambda:replace-text control from to "replacement λ")
```

It replaces the half-open native range `[from,to)`, inserts when endpoints are equal,
and removes when the replacement is empty. It returns the supplied replacement
string. Use silent whole-value SETF when notification is unwanted. Native caret,
selection, modified state and undo effects follow the port; no stable undo/modified
API or exact no-op callback count is promised. Callback conditions re-signal only
after native code returns; the mutation can already have happened. A handled error
allows subsequent edits. Normal native callback and deferred-destruction protections
apply, including edits invoked from callbacks.

## Native offsets

```lisp
(wxlambda:text-last-position control) ; native end offset
(wxlambda:text-insertion-point control)
(setf (wxlambda:text-insertion-point control) 0)
(multiple-value-bind (from to) (wxlambda:text-selection control)
  (wxlambda:replace-text control from to "λ"))
(setf (wxlambda:text-selection control)
      (list 0 (wxlambda:text-last-position control)))
```

`text-selection` returns two integer values; its SETF writer accepts a proper
list of exactly two integers and returns that same list. Setting the insertion
point collapses selection according to native behavior. All accepted positions
are nonnegative signed-64-bit integers, further bounded by the control's current
native last position. Selection/replacement requires ordered endpoints. Invalid
ranges signal, without native clamping; the wxWidgets `-1` select-all/end sentinels
are not exposed. Pass the actual end offset instead.

These are **native offsets, never portable Lisp string indices**. Unicode encoding
units and native CRLF bookkeeping can make them differ from `length`, `subseq` or
character/grapheme counts. Obtain offsets from the control and pass them back to
native editing APIs. Selection values apply to current content; edits can invalidate
saved ranges. This slice does not expose GetRange, text-coordinate conversion,
selection clipboard operations, Cut, undo/redo or arbitrary style flags. The
[clipboard](CLIPBOARD.md) remains a separate whole-string API.

## Example and verification

`examples/text-editor.lisp` creates a multiline editor, appends lines at the native
end, replaces its current selection and toggles user editability. Its status reports
native end offsets. The existing persistent form remains a single-line form;
its four-line document format is not a multiline editor format.

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/text-editor.lisp --eval '(wxlambda-text-editor:start)'
sbcl --load ~/quicklisp/setup.lisp --load examples/text-editor.lisp --eval '(wxlambda:with-application () (wxlambda-text-editor:exercise))'
```

The bounded exercise selects/replaces Unicode multiline text, receives native
text-change callbacks and checks read-only state before cleanup. `tests/text-editing.lisp`
adds defaults/direct/factory subclasses and AFTER-hook rollback, silent SETF versus
notifying replacement, native selection/caret/end, invalid ranges/type/NUL/overflow,
callback error recovery, nested borrowed-context suspension, callback-time
disposal and stale-resource checks. The native harness checks raw
UTF-8/flags/output pointers, wrong/stale handles and bounds without wx assertions.
Physical typing/selection, platform-specific IME/grapheme behavior, hosted Windows/
Linux and a complete multiline document application remain separate work. See
[coverage](WXWIDGETS-COVERAGE.md), [CLOS API](CLOS-API.md) and
[Cadre installation](DOCSET.md).
