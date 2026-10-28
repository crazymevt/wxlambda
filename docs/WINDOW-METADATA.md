# Window names and frame titles

This coverage slice adds logical window names and mutable frame titles.
These are CLOS generic functions; names work with every currently bound
window/control. Title operations accept
frames; other top-level window types remain future work.

## Logical names

```lisp
(setf (wxlambda:window-name window) "search-input") ; returns the supplied string
(wxlambda:window-name window)                    ; independent string
```

A name is a logical string property, separate from a control's displayed label,
its text value, a frame's title and the native integer ID. Names need not be
unique. Changing a name does not synthesize a command event or change the label.
Unicode, empty names and embedded NUL characters are preserved using explicit
UTF-8 byte counts across the bridge. A setter copies its input; changing the
Lisp string afterwards does not change the native name. A getter returns an
independent Lisp string that survives later queries and window destruction.

## Frame titles

```lisp
(wxlambda:frame-title frame)                     ; includes initial make-frame title
(setf (wxlambda:frame-title frame) "Results — λ")  ; returns the supplied string
```

Title setters update the existing frame, and getters return independent strings.
Empty titles and Unicode are accepted. Display and platform normalization follow
wxWidgets and the native window system. A frame's logical name is unaffected.
These operations expose `wxTopLevelWindow::GetTitle` and `SetTitle` for the
currently bound `wxFrame` type; passing a panel or control signals a Lisp dispatch error.

All four functions require a live window in an active main-thread GUI session.
Invalid/destroyed handles and inactive sessions signal `native-error`; wrong
Lisp argument types signal Lisp conditions. Empty successful results are strings,
not failure sentinels. No parent ownership or handler lifetime rules change.

## Example and checks

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/window-metadata.lisp \
  --eval '(wxlambda-window-metadata:start)'
```

The example constructs a subclass directly with `make-instance` and counts
title updates using an `:after` method on `(setf frame-title)`. Click the button to update both the frame name
and its Unicode title. Close the
frame to exit. The example's reusable `exercise` queues that same callback,
checks the resulting properties and subclass hook, explicitly unbinds it and
destroys the frame. See [the CLOS API guide](CLOS-API.md) for the dispatch and
subclassing contract.
The automated test runs this example, checks names on all seven widget types,
independent labels/text, Unicode/empty/NUL names, saved string lifetime,
non-frame title errors, invalid native pointers and destroyed/inactive handles:

```sh
sbcl --noinform --no-sysinit --no-userinit --non-interactive \
  --load ci/setup.lisp --load tests/window-metadata.lisp
ccl --no-init --batch --load ci/setup.lisp --load tests/window-metadata.lisp
```

Native ABI 9 adds length-aware metadata strings. Rebuild the bridge with the
updated Lisp bindings. Getter pointers are borrowed until the next name/title
query on the same thread; the public Lisp getters copy them immediately. Existing
text and label getter buffers retain their previous lifetime contracts.
