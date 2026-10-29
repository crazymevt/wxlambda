# Focus navigation for keyboard-driven forms

ABI50 adds generic `focus-window` and `navigate-focus` on public WINDOW wrappers.
Both require a live, shown-on-screen, enabled window on the GUI thread. Hidden,
disabled (including ancestors) and destroyed receivers signal NATIVE-ERROR.

```lisp
(wxlambda:focus-window name-field :from-keyboard t)
(wxlambda:navigate-focus name-field) ; forward
(wxlambda:navigate-focus name-field :forward nil) ; backward
```

`focus-window` returns its receiver after native SetFocus, or SetFocusFromKbd
when FROM-KEYBOARD is non-NIL. Container SetFocus may delegate to a child.
Keyboard-origin focus may select editor text according to native control policy.
A request does not guarantee OS activation, delivery or a specific focused child.
`navigate-focus` returns T/NIL from native Navigate and starts at the receiver's
parent traversal container. FORWARD defaults T; NIL selects backward traversal.
Both keywords accept generalized booleans. Native eligibility, order and wrapping
vary by port and container. No tab-order mutation, NavigateIn, window-change flag,
focus event resources, raw focus pointer or C++ virtual hooks are exposed.

Focus changes can synchronously dispatch native focus/navigation events. Calls
from Lisp event callbacks reject; enqueue an owner-bound deferred UI action:

```lisp
(wxlambda:on-click return-button
  (lambda ()
    (wxlambda:defer-ui-action
      (lambda () (wxlambda:focus-window name-field)) :owner name-field)))
```

During the native operation, bridge destruction/close of any session window is
rejected, because the previous focus and traversal siblings can be involved.
Shared Input operation guards also block nested GUI pumps/RUN, modal/clipboard
entry and safe-idle delivery. Recursive focus operations reject. Callback errors
re-signal at the Lisp boundary without rolling native state back. This does not
intercept arbitrary foreign native deletion or change OS policy.

Run [focus-navigation.lisp](../examples/focus-navigation.lisp) using its indexed
command in the docset. The deterministic exercise issues requests and uses a
deferred command; physical Tab/Shift-Tab, OS activation, accessibility and hosted
ports require separate interactive verification. Native baseline: wxWidgets3.2.12
[window declarations](https://github.com/wxWidgets/wxWidgets/blob/v3.2.12/interface/wx/window.h).
See [deferred actions](DEFERRED-UI-ACTIONS.md) and [Cadre installation](DOCSET.md).
