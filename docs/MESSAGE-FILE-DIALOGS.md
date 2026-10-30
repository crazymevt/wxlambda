# Generic message and single-file dialogs

ABI 34 adds MESSAGE-DIALOG and FILE-DIALOG using **wxGenericMessageDialog and
wxGenericFileDialog**: native wxWidgets GUI controls with a guarded modal bridge.
The choice is explicit in this API's scope; there is no :BACKEND argument.
Platform-native alerts/file panels, including macOS NSAlert/NSPanel, remain
unbound after their cancellation probe hung. This generic implementation does
not claim native OS-dialog parity. The baseline is wxWidgets 3.2.12.

The file and nested directory dialogs use symbol-isolated private copies of
the pinned wx generic implementation. Some wxGTK/MSW builds neither export nor
install these generic classes; copying an absent header or switching to native
panels would not preserve this modal contract. The bridge compiles its copies on
all ports, without globally renaming foreign wx classes or importing their private
symbols. Original copyright/licence notices are retained under
`native/generic-dialogs/`; `tools/generic_dialogs.py` verifies upstream hashes and
reproduces the private adapters. The Lisp/FFI interface and behavior below are
unchanged. Real native button acceptance, nested prompts and cleanup remain
covered by the native regressions as well as the runnable example.
Builds require wx file/directory dialogs, file controls, tree controls, bitmap
buttons and static lines; configuration rejects builds missing these prerequisites.

Both classes inherit DIALOG/WINDOW and the [modal lifetime protocol](STANDARD-DIALOGS.md).
Keyword-only MAKE-MESSAGE-DIALOG/MAKE-FILE-DIALOG are thin ordinary factories
with validated :CLASS. Direct construction, subclass slots/defaults/AFTER hooks,
rollback and optional parent normalization work like the earlier dialog families.
A requested hidden parent remains the actual native parent; NIL is truly
parentless. Construction is allowed on the GUI thread in callbacks/RUN/pumps.
Receiver/ancestor pending and scheduled deletion rejects before modal entry.
Strings must be Unicode without NUL; stale handles and wrong native state reject.

## Message body, buttons and results

```lisp
(let ((dialog (wxlambda:make-message-dialog
               :parent frame :title "Unsaved changes"
               :message "Save this document?" :buttons :yes-no-cancel
               :icon :question)))
  (unwind-protect
       (case (wxlambda:show-modal dialog)
         (:yes (save-document))
         (:no (discard-edits))
         (:cancel nil))
    (wxlambda:destroy-window dialog)))
```

:BUTTONS is :OK (default), :OK-CANCEL, :YES-NO or :YES-NO-CANCEL. :ICON is :NONE,
:INFORMATION (default), :WARNING, :ERROR or :QUESTION. DIALOG-MESSAGE reads the
fixed message body and MESSAGE-DIALOG-BUTTONS reads the offered button set.
DIALOG-TITLE remains separately mutable; message body/buttons/icon have no
writers in this slice.

SHOW-MODAL can return :YES/:NO for a message dialog offering those buttons, in
addition to :OK/:CANCEL where applicable. END-MODAL validates requested results
against the dialog's offered buttons; :CANCEL is always available as a safe
completion/unwind result, including a dialog without a visible Cancel button.
The operation remains raw native result assignment, not a simulated button click.

## Configure one file selection

```lisp
(let ((dialog (wxlambda:make-file-dialog
               :parent frame :mode :open :title "Open Lisp source"
               :wildcard "Lisp files|*.lisp|All files|*" :filter-index 0)))
  (unwind-protect
       (when (eq :ok (wxlambda:show-modal dialog))
         (open-document (wxlambda:file-dialog-path dialog)))
    (wxlambda:destroy-window dialog)))
```

:MODE is :OPEN (default) or :SAVE, readable through FILE-DIALOG-MODE.
The default title is "Open file" or "Save file" according to mode.
:DIRECTORY and :FILENAME default to empty strings; an empty directory lets native
code use the current directory. A supplied directory must exist. :DIRECTORY and
optional :PATH accept strings or pathnames, converted through native namestring;
:FILENAME is a string. A supplied path must be nonempty and its directory, when present, must exist. The wrapper does not
request wxFD_CHANGE_DIR.

:MUST-EXIST defaults true for open and NIL for save. :OVERWRITE-PROMPT defaults
true for save and NIL for open. True :MUST-EXIST is rejected for save; true
:OVERWRITE-PROMPT is rejected for open. These configure actual native OK-button behavior.
END-MODAL :OK does **not** enforce existence/overwrite checks, validation or data
transfer. A programmatically assigned success result is not evidence of accepted
file selection. Application code should still handle its own subsequent IO errors.

:WILDCARD defaults to "All files|*" and requires nonempty description/pattern
pairs, such as "Lisp files|*.lisp|All files|*". :FILTER-INDEX defaults zero and
must identify a pair. FILE-DIALOG-FILTER-INDEX reads/writes that zero-based index;
no wildcard/item-list mutation API is supplied.

FILE-DIALOG-PATH reads the current native path as a string. Its SETF accepts a
nonempty string/pathname and returns the original supplied value; the getter can
return a native-normalized string. Current path is not an accepted-result flag.
The file dialog selects one file; multiselect, external/custom controls and raw
native panel adoption remain outside this scope.

The wrapper does not read or write selected file contents. Native browser actions
can have filesystem effects: for example, New Directory can create a directory
immediately. Do not interpret a picker as guaranteeing zero filesystem changes,
or assume Cancel reverses a browser action. Configured existence checks happen
through actual UI acceptance; neither constructing a path nor reading one proves
that its target exists or will remain accessible.

## Internal prompts and safe cancellation

The generic file UI can open internal message prompts, for example during native
overwrite/validation/browser actions. The bridge redirects those prompts to the
generic message implementation and tracks their modal depth. Borrowed contexts
are suspended across each internal modal boundary. These are bridge-controlled
native prompts; nested **public** SHOW-MODAL calls remain forbidden.

A callback error at a newly entered native modal boundary cancels the whole
workflow and re-signals the primary Lisp condition after unwind. Internal prompts
are ended before the outer dialog, sequentially; public END-MODAL cancellation
uses the same order. While an internal prompt is active, a public request to
successfully end the outer dialog (:OK/:YES/:NO) rejects. Only public :CANCEL
requests whole-workflow unwind. Ordinary user Cancel on an inner prompt returns normally to
the outer dialog. A notifying operation whose error is handled locally at the
same modal depth can recover without cancelling the workflow. Until a captured
callback condition reaches its Lisp boundary, a private error handoff latch
prevents native code from opening another prompt with callbacks suppressed.
The latch clears before the primary condition is re-signaled, so handled errors
can resume normal prompting. SHOW-MODAL and construction drain a pending primary
condition before opening UI or allocating a new dialog. This is an internal
safety protocol, not a public FFI or error-state API. Property calls
that trigger native internal prompts use this protection too. Cancellation
requested before modal-loop entry is remembered rather than opening a stranded
loop.

Current-dialog close cancels; ancestor close is consumed, and explicit
ancestor/dialog destruction remains blocked until the outer SHOW-MODAL returns.
Manual pumps/RUN/finish and structural menu edits remain guarded. Use ordinary
UNWIND-PROTECT cleanup after modal return. No worker-thread modal dispatch,
platform-native cancellation backend or native Lisp virtual override is added.

## Example, implementation dependencies and next workflow

[message-file-dialogs.lisp](../examples/message-file-dialogs.lisp) supplies
WXLAMBDA-MESSAGE-FILE-DIALOGS:EXERCISE inside an active GUI session. It checks
message buttons/file mode and cancels actual generic message/file modal loops
from native idle, including a message without a visible Cancel button:

```sh
sbcl --load ~/quicklisp/setup.lisp --load examples/message-file-dialogs.lisp \
  --eval '(wxlambda:with-application () (wxlambda-message-file-dialogs:exercise))'
```

START is interactive: it asks whether to choose a file, then prints an accepted
selection; it does not read the selected contents. Replace the eval expression
with `(wxlambda-message-file-dialogs:start)` for that manual workflow. The current
[handoff](../IMPLEMENTATION.md) records exact automated/manual evidence and gaps.
Final regression, package and installed-docset checks remain batch-specific.

The bridge depends on generic wxWidgets implementation headers and the generic
internal-message hook. It preserves the requested file save/overwrite style flags
when generic wxDialog style normalization overlaps them on macOS. These bounded
implementation dependencies do not expose a public raw style or hook interface.
The public source inventory maps the supported inherited/equivalent message/file
surface partially; generic implementation classes absent from the pinned public
interface are not invented as fully covered declarations.

Next compose a form/editor open/save workflow with menus and these dialogs,
keeping file-content IO and the document model Lisp-native. Graphical platform
conventions, actual user acceptance, overwrite/browser operations and hosted
Linux/Windows behavior require their own evidence. The earlier native OS probe
remains an explicit gap rather than a claim that every standard dialog is done.

ABI43 [deferred UI actions](DEFERRED-UI-ACTIONS.md) provide safe FIFO work after
callbacks, including modal/clipboard/destructive requests. Existing nested event
callback guards still apply; pending-only dispatch does not run the action queue.

The [persistent document form](DOCUMENT-FORM.md) now composes these APIs with
deferred shared Open/Save/Copy/Paste/close requests, toolbar/keyboard entry points
and status feedback. Its coalescing and scoped close-replay policy are application
logic; no extra public binding or native parity claim is introduced.
