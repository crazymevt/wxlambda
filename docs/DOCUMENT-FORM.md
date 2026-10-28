# Persistent form workflow

`examples/document-form.lisp` composes the existing controls, frame-owned menus,
generic message/file dialogs and vetoable close events into a persistent form.
The form composes the existing public APIs; clipboard actions use ABI35 text
clipboard support.
Document models, validation and file content I/O belong to the Lisp application.
This is a representative form workflow, not a multiline text editor.

## Run

From the repository root, with wxWidgets and Quicklisp installed:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/document-form.lisp --eval '(wxlambda-document-form:start)'
```

Edit the name, greeting checkbox and style choice. File offers Open, Save,
Save As and Close. The title's star compares current values to the saved or
loaded baseline; reverting all changes clears it. Save reuses the current path;
Save As asks for a destination and uses the generic chooser's overwrite prompt.
Regular Save replaces its existing destination without another overwrite prompt.
Paths from the native chooser use `uiop:parse-native-namestring`, preserving
literal native filename characters rather than interpreting Lisp pathname escapes.

When Open or a vetoable Close would discard changes, Yes saves first, No discards,
and Cancel keeps the current form. Canceling the save chooser or failing to save
also blocks that operation. Canceling the open chooser leaves current data intact;
if Yes already saved changes, that save remains completed. A complete successful
read precedes replacement of the form's values/path/baseline. Read, parse and write
errors display a generic error dialog and leave the document state unchanged.
Nested menu commands are ignored while an operation is active; vetoable Close is
blocked until it finishes. Native modal guards also protect an active dialog's
parent. Non-vetoable/forced closing follows native default policy without prompting
and can lose unsaved changes. Session shutdown is not an autosave facility.

Edit / Copy name copies the whole name and attempts best-effort persistence;
Paste name reads text directly, accepts empty text, rejects multiline input and
updates dirty state. No available text leaves the name unchanged. Copy replaces
all system clipboard formats; these are whole-field operations, without native
selection editing or Cut. See [text clipboard](CLIPBOARD.md).

## File format and write scope

The UTF-8 `.wxform` format contains exactly four lines:

```text
WXLAMBDA-FORM-1
Ada
yes
friendly
```

The third line must be `yes` or `no`; the fourth must be `friendly` or `formal`. The name may be
empty but must fit on one line. Trailing extra lines are rejected. The reader does
not call the Lisp reader or evaluate file contents. It is intended for small local
form documents, without file-size limits or streaming large-document support.

Writes create a unique temporary sibling, write and close it completely, then use
`uiop:rename-file-overwriting-target`. Failed replacement attempts clean up their
temporary sibling; earlier document state is retained. This reduces exposure to
partial content writes but promises neither crash durability nor atomic replacement
on every platform. Replacement can change permissions/metadata and replace a
symlink itself. Concurrent external edits are not detected; Save can overwrite
them. There is no locking, backup, undo, recovery or transactional filesystem API.
Use it for ordinary user-selected local documents rather than privileged files.

## Automated exercise and checks

The bounded example exercise uses a scripted canceled save, without touching files:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/document-form.lisp --eval '(wxlambda:with-application () (wxlambda-document-form:exercise))'
```

`tests/document-form.lisp` runs the example, isolated UTF-8 file round trips,
literal native filenames, menu Save, failed/malformed/missing files, failed staged
replacement cleanup, cancellation and save-before-open decisions, dirty-state
reversion, nested-command rejection and canceled/failed-save close vetoes. It also
cancels real generic save, unsaved-close and error dialogs from native idle, checking
that the outer close context resumes safely. Scripted chooser/confirmation/error
functions are example-level test seams; no new library API is introduced.

The complete runner includes this scenario. Native inventory statuses stay factual:
this composition does not implement `wxDocument`, `wxDocManager`, native file
streams or native OS alerts/panels. Graphical physical menu/chooser input and hosted
Windows/Linux behavior require separate verification. See [message/file dialogs](MESSAGE-FILE-DIALOGS.md),
[menus](MENUS.md), [coverage policy](WXWIDGETS-COVERAGE.md) and
[Cadre installation](DOCSET.md).
