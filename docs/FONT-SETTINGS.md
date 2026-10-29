# Font settings (ABI65)

`available-fonts` returns sorted copied installed face names. `:fixed-width-p t`
filters using native font enumeration. `default-font` returns `(face points)` and
fixed-width status; normally it queries the OS GUI font, while `:fixed-width-p t`
requests a13-point TELETYPE family font. Applications can use this native family
for a portable editor default rather than assuming Menlo/Consolas is installed.

`window-font` reads `(face points)`. SETF accepts that pair (size6..72) or NIL to
reset the OS GUI default. `:recursive-p t` updates current descendants, invalidates
best sizes and refreshes. Choice/list selections are snapshotted before any
font change and restored with item events blocked, including wxOSX peer
recreation. Re-layout after changing fonts. Platform substitution
may occur for unavailable faces. These are copied values, not owned font handles.
Reads require the main GUI thread; writes reject callbacks and retain existing
InputOperation/weak-window lifetime guards. UI widgets may use native chrome fonts.

STC text uses its own styles: set `stc-style-font`/`stc-style-size` on default32,
then `clear-stc-styles`; line numbers33 inherit the same font. Later color-only
changes to33 retain it. `stc-tab-width` is1..16 columns and does not rewrite text.
AUI palette replacement now copies its notebook's GUI font before cloning art,
so UI text and editor text can use separate faces/sizes without clipped captions.

`examples/font-settings.lisp` exercises enumeration, defaults, recursive GUI fonts,
editor/gutter style readback and tab width. Local checks run on macOS; Windows
and Linux font enumeration, substitution and physical rendering remain unverified.
