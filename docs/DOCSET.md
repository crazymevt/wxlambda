# Cadre docset

The primary documentation artifact is `docs/docset/`. It contains the public
wxlambda API reference, guides and explained example sources together, in the
`docset` library's format version 1 (`manifest.sexp`, `index.sexp`,
`entries.sexp`). The generated files are committed so readers can use them
without building the bridge or opening a GUI.

## Build and check

Requirements: Python 3, SBCL and the shared
[docset library](https://github.com/crazymevt/docset). The baseline used here is
commit `481f57e7222925c3157e41a7e3deaeb628ecc9a7` (format version 1).
Check it out beside wxlambda as `../docset`, or set
`WXLAMBDA_DOCSET_SOURCE` to its directory. The library has no runtime dependencies;
it is a documentation tool dependency, not a wxlambda runtime dependency.

```sh
python3 tools/docset.py build
python3 tools/docset.py check
```

Use the project quiet-command wrapper when running these in an agent session.
`--library /path/to/docset` overrides the library location and `--lisp` selects
the SBCL executable. Builds do not load the native bridge or start a GUI.

The build requires one authored reference record per public export, derives
function and macro lambda lists from the implementation, converts README, the implementation roadmap and
`docs/*.md` guides into docset blocks, and embeds every `examples/*.lisp` source
with usage notes. Agent instruction files (`AGENTS.md` and `CLAUDE.md`) are
excluded from the guide list. Bundled guides and examples must not contain
machine-specific paths. Update `docs/api-reference.json` and
`docs/example-reference.json` with implementation changes. API entries must state
arguments, returns, behavior, errors/scope and the related wxWidgets APIs.

Both commands run `docset:check-docset` and require zero problems, then exercise
the same discovery, symbol lookup and indexed entry reader used by Cadre.
Every API entry and guide/example entry is read back and compared with source
content; `check` also rejects stale generated content. Cadre's graphical viewer
is a separate manual integration check; shared-reader success alone is not a
claim that the viewer was manually tested.

## Install for Cadre

```sh
python3 tools/docset.py install
```

This verifies the build, copies the three files into
`$XDG_DATA_HOME/docsets/wxlambda/` (default `~/.local/share/docsets/wxlambda/`),
and verifies discovery and reading there. Use `--destination /path/to/wxlambda`
for an isolated installation. Only the three generated files are overwritten.
The install command does not alter Cadre's source or its bundled Common Lisp docs.

Open Cadre after installation, or restart it if it already cached its docsets.
In Lisp source, use Look Up in Docs/HyperSpec on `wxlambda:make-frame` to open
its API entry. Search for `README` or an example filename such as `hello.lisp`
to open guides and example source. Use the docset table of contents for the
API reference, Guides and Examples groups.

## Maintaining the artifact

Run examples affected by a change, build the docset, and run `check`. Commit all
three generated files alongside source/reference changes. The generator uses
only the tracked reference, guides and examples. It adds no build timestamps or
machine-specific output paths; source text is preserved. A repeated build from identical sources produces identical bytes.
The checker verifies internal references and index positions; external guide
links still require ordinary documentation review. Links between bundled guides
and examples become docset references. Links to repository files that are not
bundled are shown as source paths, without broken relative hyperlinks.
