# Window tooltips for form guidance

ABI51 adds generic `tooltip-text` and its SETF writer on public WINDOW wrappers.
The native window owns its tooltip; Lisp receives copied text, never a tooltip
pointer or separately disposable resource. Baseline wxWidgets 3.2.12 requires
wxUSE_TOOLTIPS=1 at build time (CMake rejects disabled support).

```lisp
(setf (wxlambda:tooltip-text name-field) "Enter your display name — λ is welcome.")
(wxlambda:tooltip-text name-field) ; fresh native text string
(setf (wxlambda:tooltip-text name-field) "") ; attached empty tooltip
(setf (wxlambda:tooltip-text name-field) nil) ; remove native tooltip
```

The reader returns NIL when no tooltip is attached and a fresh string otherwise,
including the empty string. The writer returns its supplied value. Strings are
copied into native storage, accept 0..4096 UTF-8 bytes and reject embedded NUL;
this byte limit is not a Lisp character count. Invalid inputs leave existing
text intact. Newlines are accepted; native normalization/rendering and hover
popup eligibility, timing, placement, wrapping and composite-control behavior
remain port-specific. Readback reports native text rather than a Lisp shadow.
A successful setter does not promise that a tooltip is visible or will appear.
Tooltips complement visible labels and validation feedback.

Both operations require a live window in the active main-thread session. Hidden
or disabled windows may store/read tooltip text. Reads are allowed in callbacks;
writes (including removal) reject callbacks. Queue an owner-bound action:

```lisp
(wxlambda:on-click guidance-button
  (lambda ()
    (wxlambda:defer-ui-action
      (lambda () (setf (wxlambda:tooltip-text name-field) nil)) :owner name-field)))
```

Synchronous writes protect the receiver and ancestors from bridge destroy/close
and reject recursive tooltip writes and same-window operations using the shared
Input guard. Nested GUI/RUN/pumps, modal/clipboard entry and safe-idle delivery
are also blocked during the operation. Native callback
conditions re-signal at the Lisp boundary without rolling native state back.
Windows invalidate normally on owner destruction; no Lisp tooltip root needs
cleanup. Native-ready subclass AFTER hooks may set tooltips, and existing CLOS
rollback cleans the native window if a later initializer fails. There is no
common tooltip constructor initarg.

This slice exposes no native wxToolTip class/adoption, global enable/delay/width,
rich tooltip, per-item tooltip or tooltip event API. See the
[example](../examples/tooltips.lisp), [CLOS API](CLOS-API.md),
[deferred actions](DEFERRED-UI-ACTIONS.md) and [Cadre installation](DOCSET.md).
The example's EXERCISE checks text and deferred remove/restore without hover
simulation. Physical hover, accessibility, native popup presentation, hosted
Windows/Linux/Qt and graphical Cadre require separate interactive verification.

Native reference: [wxWindow tooltip declarations](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/window.h#L3142).
