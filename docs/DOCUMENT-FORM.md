# Persistent form workflow

`examples/document-form.lisp` composes controls, frame menus, a toolbar/status bar,
keyboard accelerators and owner-bound deferred UI actions into a persistent form.
It uses existing ABI43 APIs, adding no public bindings or native document framework.
Document models, validation and UTF-8 file IO belong to the Lisp application.
This is a small whole-field form workflow, not a multiline text editor.

## Run and shared actions

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/document-form.lisp --eval '(wxlambda-document-form:start)'
```

Edit the name, greeting checkbox and style choice. File offers Open, Save,
Save As and Close; Edit offers Copy name/Paste name. The toolbar provides the
same six actions, with help text; an application-owned status bar reports results.
Menus, tools and the frame accelerator table share portable IDs12101..12106:

| Action | Shortcut |
| --- | --- |
| Open | Ctrl/Command+O |
| Save | Ctrl/Command+S |
| Save As | Ctrl/Command+Shift+S |
| Close | Ctrl/Command+W |
| Copy whole name | Ctrl/Command+Shift+C |
| Paste whole name | Ctrl/Command+Shift+V |

Logical Control means Command on macOS. Ordinary Ctrl/Command+C/V retain the
focused control's native selection-editing behavior; the form shortcuts deliberately
add Shift for whole-name operations. Explicit frame shortcuts do not add native
menu-label accelerators. Physical input, focus and system shortcut conflicts
need platform verification. The toolbar uses the library's safe in-window macOS
backend; native Cocoa toolbar chrome remains excluded.

Each UI command queues an owner-bound action and consumes its command event.
Toolbar events consume before frame propagation, preventing duplicate requests.
The first request wins while any command is pending or executing: subsequent
requests, even different commands, are ignored rather than queued for later.
This includes a close request while Open/Save is pending; request close again
after the current operation finishes. Menus/tools disable during execution and
re-enable on cancellation/failure if the frame survives. They remain enabled
while awaiting delivery because same-toolbar callback state writes are guarded.
The queue latch coalesces requests during that interval.

RUN/full GUI polling delivers work after native callback/operation stacks return;
pending-only event processing does not. Values, clipboard contents and dirty
state are read during execution, not captured at click time. Edits before delivery
therefore affect the requested save/close/paste. Nested modal commands are ignored.
Disposal cancels pending work; the weak owner also suppresses work after frame
or ancestor destruction. A canceled ticket cannot leave the request latch stuck.
The form's internal synchronous execution helpers are test/implementation seams;
all GUI entry points use the deferred request path.

## Persistence, clipboard and close policy

The title's star compares current values with the saved/loaded baseline; reverting
all edits clears it. Save reuses the current path. Save As asks for a destination
and uses the generic chooser's overwrite prompt. Regular Save replaces its current
destination without another prompt. Chooser paths use `uiop:parse-native-namestring`,
preserving native filename characters rather than Lisp pathname escapes.

When Open or a vetoable Close would discard changes, Yes saves first, No discards,
and Cancel keeps the form. Canceling the save chooser or failing to save blocks
the destructive operation. Canceling the open chooser leaves current data intact;
if Yes already saved, that save remains completed. A complete successful read
precedes replacement of values/path/baseline. Read/parse/write errors report through
a generic error dialog and preserve the document state. Reporter errors also clear
busy/active state and restore surviving command views before propagating outward.

A native vetoable close is immediately vetoed, without opening a dialog inside
its borrowed callback. Its deferred action asks/save-checks, then retries close
with a scoped, one-use approval. The close handler consumes that approval and
allows native default handling; unwind cleanup removes any unused approval. A
preceding observed veto from another handler is consumed and preserved. A blocked
retry cannot authorize a future close without a new decision. Arbitrary handler
ordering/default policies remain the library's existing contract; close acceptance
is not a destruction guarantee.

Consequently, the initial `close-window` call returns NIL even for a clean form
that may close after delivery. Tests/polling callers inspect the eventual frame
liveness and file result after processing events. Forced/non-vetoable close follows
native default policy without prompting and cancels pending form work; it can lose
unsaved changes. Native modal-parent protections still apply. Session shutdown is
not autosave, and the example does not override OS shutdown policy.

Copy name copies the whole field and attempts best-effort persistence. Paste
reads text at delivery, accepts empty text, rejects multiline input before changing
the form, and updates dirty state. No available text leaves the name unchanged.
Copy replaces all clipboard formats; there is no whole-field Cut or implicit
selection editing. See [text clipboard](CLIPBOARD.md) and
[deferred action boundaries](DEFERRED-UI-ACTIONS.md).

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

The bounded example exercise requests toolbar Save, verifies deferral and pumps
a scripted canceled chooser without touching files or system clipboard text:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/document-form.lisp --eval '(wxlambda:with-application () (wxlambda-document-form:exercise))'
```

`tests/document-form.lisp` retains isolated UTF-8 round trips, literal filenames,
failed/malformed/missing files, staged replacement cleanup, save-before-open,
dirty-state reversion, and canceled/failed-save/discard/successful-save close
outcomes. It now pumps deferred close decisions; real default save, unsaved-close
and error dialogs are automatically canceled from native idle. No old borrowed
close context is retained across those prompts.

`tests/document-form-actions.lisp` verifies menu/tool/configured shortcut-command
routing, exactly one pending request, different-command coalescing, delivery-time
values/clipboard, canceled request recovery, disabled views/active-command rejection,
file-state preservation, reporter-error cleanup, preceding close veto/approval
reset, force/disposal cancellation, and delivery under blocking RUN. Its clipboard
functions are isolated fixtures; system text is never read/replaced. The shortcut
checks validate the table and action command path, not physical typing; native
key translation is tested separately by the library's accelerator probes.

The complete runner includes both scenarios and the original clipboard protocol
scenario. No public exports, native ABI or native inventory statuses change.
Existing unbound `wxDocument`/`wxDocManager`, native streams, OS alerts/panels and
deep editor APIs remain pending/deferred. Physical menu/tool/keyboard/chooser input,
OS clipboard interoperability, hosted Windows/Linux GUI, graphical Cadre,
crash-durability and arbitrary IO-race injection remain unverified. See
[coverage policy](WXWIDGETS-COVERAGE.md) and [Cadre installation](DOCSET.md).
