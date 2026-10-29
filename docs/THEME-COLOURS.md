# Theme colours (ABI64)

`system-colours` returns two fresh RGB triples for system window text/button face,
independent of application overrides. `window-colours` reads effective window
colours; SETF takes `(foreground background)` RGB triples or NIL to reset defaults.
`:recursive-p t` writes the current native subtree, including toolkit children.
Native controls may ignore explicit backgrounds or retain system-drawn chrome.

`notebook-tab-colours` takes `(base active)` packed RGB24 integers, or NIL to
restore default art. Its reader reports last configured metadata, not an art
provider object. The owned generic AUI art chooses contrasting tab text; native
close/scroll glyphs still follow system appearance. Pages/selection are retained. Replacing art immediately initializes the sizing
information in every existing private tab group, even when the strip height
stays unchanged; captions and close controls remain within the visible strip.

`stc-caret-colour` reads/writes native caret RGB24. `stc-selection-colours` takes
`(foreground background)` RGB24 or NIL for toolkit defaults. Its reader reports
last configured metadata: wxSTC offers no selection-color getter. Neither changes
text, selection range or undo history. Use existing style32/clear-stc-styles and
style33 for editor/gutter colours. These do not install a lexer.

All require an active main-thread session. Reads are callback-safe. Writes reject
callbacks and must be deferred to a safe boundary. Resources follow their parent
window lifetime. Arbitrary art providers, raw colour handles, native menu/titlebar
appearance and full platform-wide theme switching remain outside this API.

Run `tests/theme-palette.lisp` for the automatic example and callback refusal.
Local verification targets macOS; Windows/Linux native drawing remains unverified.
