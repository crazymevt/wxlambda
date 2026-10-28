# Project instructions

## Long-term goal and session continuity

The final goal is complete wxWidgets coverage, including required public APIs,
overloads, inherited behavior, optional components and applicable platforms.
A bounded first release is only an intermediate milestone. Pending, partial or
explicitly unsupported required APIs remain gaps and prevent claiming completion.
Prioritize unfinished C1 foundations before further C2 widget-family expansion.

Read the session handoff in `IMPLEMENTATION.md` and `docs/WXWIDGETS-COVERAGE.md`
when resuming. Keep the handoff current with completed slices, the next dependency,
review findings, verification evidence and limitations. Use independent code
review, resolve actionable findings, then commit verified slices.

With each binding change, update `docs/coverage/status.json` and regenerate the
summary, class index and all C1–C7 checklists with `tools/coverage_inventory.py`.
The generated `docs/coverage/README.md` is the current phase/header backlog;
do not hand-edit generated counts or checkboxes. Check regeneration against the
pinned upstream sources and rebuild/check the docset, including that summary.

## Implementation deliverables

Every implementation change must include corresponding documentation, examples,
and docset updates. Keep all three aligned with the implemented behavior and
its supported scope; examples are required, not optional.

The docset is the primary documentation deliverable. Use the format consumed by
Cadre: the `docset` Common Lisp library's **format version 1**, specified in
[docset/docs/format.md](https://github.com/crazymevt/docset/blob/481f57e7222925c3157e41a7e3deaeb628ecc9a7/docs/format.md).
Build it in `docs/docset/` as three UTF-8 Lisp-data files: `manifest.sexp`,
`index.sexp`, and `entries.sexp`. Use docset ID `wxlambda` and package
`WXLAMBDA`, with a table of contents and indexed entries readable by Cadre.

Include these three content groups together in that docset:

- **Standard library reference content:** indexed entries for every implemented
  public wxlambda export, including callable APIs, event types/accessors and
  conditions. Document syntax, arguments, return values, behavior, errors and
  supported scope as applicable; identify related wxWidgets APIs and versions.
- **Project documentation:** installation, usage, development/debugging,
  packaging guides and documented limitations.
- **Project examples:** the example source code, explanations and commands to
  run it, accessible from the docset's index and table of contents.

Generate these as Cadre-readable entries, including code blocks for examples.
Keep the standalone documentation and example sources available as well.

Before marking implementation work complete, verify the documentation matches
its public interfaces, run the relevant examples, and build and verify the
docset includes all three content groups. Run `docset:check-docset` on the
output and require zero reported problems; also verify public-export coverage,
links, and Cadre discovery, symbol lookup and access to guides/examples.
Document how to install it for Cadre (for example, under
`$XDG_DATA_HOME/docsets/wxlambda/`, defaulting to
`~/.local/share/docsets/wxlambda/`). Report the checks and any failures or checks
not run; do not claim unfinished deliverables are complete.

See `IMPLEMENTATION.md` and `docs/WXWIDGETS-COVERAGE.md` for the roadmap and
binding completion criteria. The initial Phase 7 build prerequisite is now
implemented: use `python3 tools/docset.py build`, `check`, and `install` as
documented in `docs/DOCSET.md`. Continue building and checking docset updates
for subsequent implementation work, including Phase 3 fixes and C1–C7 bindings.

## CLOS API

Use native-backed CLOS construction: concrete classes create their native widget
through inherited `initialize-instance` methods using initargs such as `:parent`,
`:label` and `:title`. Users must be able to directly instantiate subclasses with
slot initargs and `:default-initargs`; native properties are available in subclass
`:after` hooks. Keep `make-*` factories thin, with validated `:class` support and
forwarded initargs. Reject abstract/invalid construction clearly and clean up
native widgets and callback roots when managed initialization fails.

Use generic `setf` accessors as the canonical property-writing protocol.
Name mutable properties without a `get-` prefix (`text`, `label-text`,
`frame-title`, `window-name`) so reading and SETF writing use one natural name.
Window/control operations use generic functions specialized on public wrapper
classes. The project is being built from the ground up; backward compatibility
is not required. Do not add setter aliases or receiver fallbacks solely for
compatibility. Keep native handle/type checks and callback/lifetime protections.
Application lifecycle functions, keyword-only factories, macros and the FFI can
remain ordinary functions/macros. Event snapshots remain immutable structures.

Public wrapper classes are `window` (abstract), `frame`, `panel`, `button`,
`text-control`, `label`, `checkbox`, `choice` and `gauge`. Lisp subclassing does not imply
native C++ subclassing or virtual-method callbacks; those need separate bridge
support. See `docs/CLOS-API.md`.

## Shell output

Always run noisy non-interactive shell commands (including installs, builds,
tests, linters, and verbose batch jobs) through
`/Users/jessie/.local/bin/quiet-run.sh -- <command> [args...]`.
For shell pipelines or compound commands, wrap the entire operation with
`/Users/jessie/.local/bin/quiet-run.sh -- bash -lc '<script>'`, quoting safely.
Use the wrapper's status and log path; inspect only relevant excerpts of the
full log when needed to diagnose failures or retrieve results. Preserve and
respect the wrapped command's exit code.
Short, targeted reads and interactive commands may run directly. If the wrapper
is missing or unusable, report that and use bounded output as a fallback.
