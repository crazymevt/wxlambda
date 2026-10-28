# Modal text and single-choice dialogs

ABI 33 adds the abstract DIALOG window class and two concrete native-backed
families: TEXT-ENTRY-DIALOG and SINGLE-CHOICE-DIALOG. This is a bounded extension
of the form/editor GUI workflow after menus/actions. Platform-native message/file backends remain deferred after cancellation probes.
[Generic message/file workflows](MESSAGE-FILE-DIALOGS.md) now provide a separate,
explicit wxWidgets GUI implementation; this guide covers the text/choice families.
The baseline is wxWidgets 3.2.12.

## Construct a dialog and obtain a result

```lisp
(let ((dialog (wxlambda:make-text-entry-dialog
               :parent frame :message "Document name" :title "Rename"
               :value "Untitled")))
  (unwind-protect
       (when (eq :ok (wxlambda:show-modal dialog))
         (save-name (wxlambda:dialog-value dialog)))
    (wxlambda:destroy-window dialog)))
```

Construction is allowed on the GUI thread inside callbacks, RUN and pumps,
so menu actions can create dialogs; menu structural-edit guards do not apply.

The keyword-only MAKE-TEXT-ENTRY-DIALOG and MAKE-SINGLE-CHOICE-DIALOG factories
remain ordinary functions, with validated :CLASS subclass support. Direct
MAKE-INSTANCE uses the same :PARENT/:MESSAGE/:TITLE and family-specific initargs.
Parent is optional; when supplied it must be a live managed window. A receiver
or ancestor pending native destruction is unavailable even before native
IsBeingDeleted becomes true: bridge markers and the public wxApp scheduled-
deletion list both participate in validation. SHOW-MODAL rejects before entering
the native loop when the dialog or an ancestor is already scheduled for deletion.
The bridge
normalizes the actual native parent to that requested window, even when hidden.
NIL means truly parentless; native automatic parent selection is undone. Unicode
strings must not contain NUL; empty message/title/value strings are accepted.
SINGLE-CHOICE-DIALOG requires a nonempty list/vector of strings in :ITEMS.
:SELECTION defaults to zero and must remain an in-range nonnegative integer;
NIL does not mean no selection in this bounded dialog API.

The DIALOG base is abstract. Subclass slots, :DEFAULT-INITARGS and AFTER hooks
follow the native-backed CLOS protocol; native properties exist before the
subclass AFTER hook. Invalid/mixed managed construction rejects before allocation.
Initialization failure cleans newly allocated dialog resources and callback
roots, without reversing arbitrary edits to preexisting external resources.
Dialogs are managed windows: ordinary DESTROY-WINDOW, parent ownership and stale
handle checks control native lifetime. No Lisp GC finalizer owns native deletion.

## Native modal loop and scoped callbacks

SHOW-MODAL returns :OK or :CANCEL. It may run from a menu/event callback, a RUN
loop or a native pump. Only one public managed modal call may be active: nested public
SHOW-MODAL calls and already-shown dialogs reject. Guarded internal generic
file prompts are described in the separate message/file guide. Native modal processing enters its own
callbacks while SHOW-MODAL waits. The outer borrowed event context is suspended throughout SHOW-MODAL, including
intervals without an inner callback. Each inner context is valid only in its own
current innermost callback. After SHOW-MODAL returns, the outer context can resume under
its ordinary lifetime rules.

Bare callback errors are retained at the bridge boundary, cancel the modal loop
and re-signal the primary Lisp condition after native unwind. A nested notifying
operation whose condition is caught locally can recover without cancelling the
modal interaction. This does not allow Lisp conditions to unwind through C++.
Manual RUN/pump/finish and structural menu edits remain guarded while the modal
call is active; the native modal loop owns dispatch.
STOP requests exit of an outer RUN loop, if one is running; use END-MODAL to
dismiss the current prompt.

END-MODAL accepts :OK or :CANCEL, calls native EndModal directly and returns
the dialog object. It does not
simulate an OK/Cancel button, run button validation or perform native data
transfer. A programmatic :OK result therefore does not prove that native form
acceptance/transfer occurred. DIALOG-MODAL-P reports current native IsModal;
that can become false after END-MODAL before SHOW-MODAL returns. The bridge still
rejects destruction of the active dialog and all its ancestors until the modal
call has fully returned. Unwind cleanup should destroy after SHOW-MODAL exits. Native close of the current
dialog is consumed and cancels the modal loop; ancestor close events are consumed
while the modal call is active. Public CLOSE-WINDOW on a protected ancestor
rejects. This keeps a queued/native close from bypassing the lifetime guard.

These dialogs are modal-only. SHOW-WINDOW and SHOWN-P SETF reject them; the
SHOWN-P reader and supported generic window operations retain their documented
native semantics. Use SHOW-MODAL to show them. Arbitrary modeless/window-modal,
custom button/validator, native adoption and virtual Lisp-dialog hooks are outside
this slice.

## Stored values and selection

DIALOG-TITLE reads/writes the native title and permits scalar changes during
callbacks. DIALOG-VALUE reads/writes native TEXT-ENTRY-DIALOG stored value.
GetValue need not return an unaccepted live text-control edit buffer. Its writer
crosses a notifying callback boundary, so callback conditions return through the
existing Lisp protocol. Use normal user acceptance when native validation/data
transfer is part of the desired workflow.

DIALOG-SELECTION reads/writes a valid SINGLE-CHOICE-DIALOG index. The items are
fixed at construction. DIALOG-SELECTION-TEXT reads native GetStringSelection,
the last accepted native selection-string cache rather than a string derived
from the current index. SetSelection changes the index, but GetStringSelection
can stay empty until an actual OK-button transfer; a previous accepted cache can
remain after index changes or raw END-MODAL. Do not assume a programmatic :OK
result synchronizes accepted-value caches. No arbitrary clientData pointers, item mutation,
multiselect or radio/list-control ownership interface is added.

## Example, native probe and next scope

[standard-dialogs.lisp](../examples/standard-dialogs.lisp) provides a programmatic
exercise for the bounded modal protocol. START opens an interactive text prompt:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/standard-dialogs.lisp \
  --eval '(wxlambda-standard-dialogs:start)'
```

For deterministic native cancellation without user input, run EXERCISE in a
managed session:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/standard-dialogs.lisp \
  --eval '(wxlambda:with-application () (wxlambda-standard-dialogs:exercise))'
```

The exercise binds native idle, ends one actual text-dialog modal loop with
:CANCEL and checks preserved stored value and cleanup. It does not exercise
graphical acceptance or the choice dialog; those require separate evidence.
Current verification evidence is kept in the [handoff](../IMPLEMENTATION.md).

The macOS exploratory native message/file cancellation probe hung past its
five-second watchdog even when native IsModal was false; those platform-native backends remain unbound. The separate generic backend
uses guarded native wxWidgets GUI controls instead. Text-entry/single-choice native
cancellation completed promptly in the bounded probe. This is platform evidence,
not proof of Windows/Linux behavior or a public guarantee about elapsed time.

Continue the form/editor open/save workflow with the explicit
[generic message/file API](MESSAGE-FILE-DIALOGS.md), retaining platform-native
backends as a separate gap. Do not drain
unrelated native event ownership utilities. Graphical user acceptance, validation,
OS-native dialog conventions and cross-platform GUI behavior require their own
reports; the native declaration inventory remains factual and partial.

ABI37 rejects synchronous modal entry during notebook navigation/page callbacks;
defer prompts to later application work. See [notebook navigation](NOTEBOOK.md).
