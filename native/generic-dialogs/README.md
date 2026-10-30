# Private generic wx GUI dialogs

These four source/header files derive from wxWidgets v3.2.12. Original copyright
and licence notices are retained; `LICENCE.txt` and `LGPL.txt` accompany them.
They are not a new project-wide licence choice.

Some ports do not build/install the generic file/directory dialog classes, even
though those sources exist upstream. wxLambda must not replace its guarded wx
GUI backend with platform-native panels to work around missing headers.

`tools/generic_dialogs.py` verifies the SHA256 of all four original inputs and
reproduces these private adapters. Changes are limited to symbol-isolated
`WxlGenericFileDialog`/`WxlGenericDirDialog` names, private header guards/includes,
removal of DLL-import/export decorations for these two private classes only,
omission of the native `wxFileDialog` proxy, and an explicit directory-dialog
base header include. Behavior/event handling remains pinned upstream code.
Other wx classes retain their original DLL import declarations. No preprocessor
macro globally renames foreign wx classes.

Both the library and native test harness compile these sources on all ports.
There is no build-time download/generation and no duplicate foreign class export.
Construction, flags, modal hooks, nested prompts, cancellation and lifetime guards
continue through the existing bridge. No new Lisp/FFI surface is introduced.

To regenerate/check against a pinned source checkout:

```sh
python3 tools/generic_dialogs.py /path/to/wxWidgets
python3 tools/generic_dialogs.py /path/to/wxWidgets --check
```

Do not change upstream behavior without separately auditing modal/lifetime policy
and updating the reproducible transformation, docs and regressions.
