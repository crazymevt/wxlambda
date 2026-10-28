# Text clipboard

ABI35 exposes text on the standard system clipboard through three public names:

```lisp
(wxlambda:clipboard-text-available-p) ; T/NIL, advisory only
(wxlambda:clipboard-text)            ; independent string, or NIL for no text
(setf (wxlambda:clipboard-text) "Hello λ") ; returns that same Lisp string
(wxlambda:flush-clipboard)           ; T if persistence succeeds, NIL otherwise
```

All operations require an active application on the main thread. `clipboard-text`
and its SETF writer are generic functions, following the property protocol even
though the clipboard has no public receiver object. Availability and flush are
ordinary functions. Each operation briefly opens the native clipboard, closes it
on completion/failure and restores any preexisting primary-selection preference.
The API always selects the standard clipboard, not X11 PRIMARY. It rejects an
already-open clipboard rather than closing a lock owned by other native code.
Reentrant clipboard access is rejected; outer borrowed event contexts are suspended
across a native transfer and resume afterward. Callback conditions remain contained
at the C boundary and re-signal after lock cleanup, taking precedence over native
transfer errors. As with other native operations, earlier side effects are not
rolled back when a callback condition escapes.

## Text and persistence

NIL means no supported native text is present; `""` is available empty text. A
failed open or supported-text transfer signals `native-error`, rather than returning
NIL. Availability can change before a later read; Paste should read directly and
handle NIL/errors. Reads copy UTF-8 into an independent Lisp string. Unicode text
is preferred, with the native legacy text format accepted as fallback; conversion
and line endings follow the wxWidgets port. Embedded NUL is rejected on reads and
writes. Non-string writes have no applicable SETF method. The setter replaces all
prior clipboard formats and transfers a text data object to wxWidgets; it does not
append or preserve images, HTML, files or other formats. There is no automatic
clipboard restoration or transactional rollback of failed native writes.

Writing does not imply persistence after the application exits. `flush-clipboard`
is a separate best-effort request and returns NIL when unsupported or unsuccessful;
a valid false result is not `native-error`. The pinned documentation identifies
MSW/GTK persistence support, with port-specific differences; no guarantee is made
for macOS or for clipboard contents surviving exit without a successful flush.
Open/session failures still signal. No raw Open/Close, Clear, PRIMARY selection,
data-object classes, asynchronous format callbacks, clipboard events, binary/image/
file payloads, control selection ranges or Cut are exposed in this slice.

## Examples

`examples/clipboard.lisp` has an interactive START with Copy whole field and Replace
field from clipboard buttons. Copy changes the system clipboard only when clicked.
Paste accepts a single line, including an empty string, and reports absent text or
transfer errors. Automatic EXERCISE only queries availability:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/clipboard.lisp --eval '(wxlambda:with-application () (wxlambda-clipboard:exercise))'
sbcl --load ~/quicklisp/setup.lisp --load examples/clipboard.lisp --eval '(wxlambda-clipboard:start)'
```

The [persistent document form](DOCUMENT-FORM.md) adds Edit / Copy name and Paste
name. Copy leaves dirty state unchanged; Paste replaces the whole name and compares
it against the saved baseline. No clipboard text leaves the form unchanged, while
multiline text is rejected before mutation. Clipboard errors use its existing error
dialog. These actions are not native text-control selection editing or an operating
system shortcut implementation.

## Verification boundaries

Native invariant tests substitute an in-memory `wxClipboardBase` only in the test
executable; that substitution is absent from the production bridge and has no ABI
export. They cover Unicode/empty/no-text/large transfers, ownership on success and
failure, lock cleanup, PRIMARY restoration, malformed input, legacy text fallback,
reentrancy, worker-thread rejection and flush true/false. MSW/OLE retains caller
ownership on failed SetData, unlike the reviewed GTK/macOS paths; both cleanup
policies are exercised by the test double. Hosted ports still need runtime checks.
Lisp tests cover copied storage, generic SETF return/type behavior, callback/error
precedence, native error text preservation across root cleanup and form Copy/Paste.
The only live system-clipboard operation in automatic tests is availability:
existing clipboard content/formats are neither read nor replaced. Actual external
application interoperability, live writes and exit persistence remain manual checks;
do not infer them from the isolated test double. See [coverage](WXWIDGETS-COVERAGE.md)
and [Cadre installation](DOCSET.md).
