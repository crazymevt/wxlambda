# Implementation plan

Target a usable first release with the existing controls, reliable REPL-driven
development, validated platforms, and standalone packaging. Full wxWidgets
coverage is inventoried in Phase 6; its implementation remains future work.
This is a plan; unchecked steps are not claims of implemented or verified
behavior.

## Implementation rule: documentation, examples, and docset

Every implementation change must include matching documentation, examples,
and docset updates, as required by [AGENTS.md](AGENTS.md). The docset is the
primary documentation deliverable: use Cadre's `docset` library format version 1
(`manifest.sexp`, `index.sexp`, `entries.sexp`). Standard content means the
indexed reference for every implemented public wxlambda export; include project
guides and explained, runnable example sources in the same docset. Verify these
deliverables alongside implementation; examples are required rather than optional.
The initial docset build and artifact are implemented in Phase 7 below;
release-wide platform and graphical-viewer verification remain open.

## Phase 1: Establish the release baseline

- [x] Correct README coverage and CI counts to match the GUI runner.
- [x] Record tested and intended OS/Lisp combinations separately.
- [x] Run the pinned-dependency suite and cache-switch regression checks.

Verified locally on 2026-10-28 on Apple Silicon macOS with wxWidgets 3.2.12,
SBCL 2.6.9, and CCL 1.13. The dependency fetcher verified all four pinned
checkouts. Both Lisps passed all 19 GUI scenarios and the separate three-process
pinned → Quicklisp → pinned callback regression. The runner previously had 18
scenarios; the delayed-input test below adds the nineteenth. Cache-switch checks
are not included in that count. Reproduction commands are in README.md.
Hosted-platform validation remains Phase 3 work.

**Acceptance:** Documented commands reproduce the baseline without unexplained
failures.

## Phase 2: Finish REPL-driven development

- [x] Add delayed-input tests for whitespace, CRLF, EOF, error recovery, and GUI
  responsiveness.

  `tests/repl-delayed.lisp` uses a timed Gray stream on SBCL/CCL. It verifies
  callback dispatch while input is unavailable, separate evaluation/callback
  error recovery under the explicit `:on-error :report` policy, and EOF cleanup. It
  rejects blocking reads during whitespace-only waits. Actual terminal/editor
  transport still needs broader validation.
- [x] Replace message-only error handling for interactive use with a debugger
  policy. Preserve the original condition and its available restarts; provide a
  restart that abandons the current form and returns to the application REPL.
  Investigate a backtrace fallback where debugger integration is unavailable.
- [x] Define GUI processing as paused while the debugger is active, resuming
  after recovery. Never unwind through a foreign callback.
- [x] Test recovery from evaluation errors and callback errors separately.

  Verified locally on 2026-10-28: both pinned-dependency SBCL/CCL suites pass all
  20 scenarios. `tests/repl-debugger.lisp` checks original condition identity,
  live evaluation restarts, abandonment without losing the session, callback
  delivery after returning through the foreign boundary, and paused/resumed GUI dispatch.
  Callback-local restarts cannot survive that safe boundary; this is documented.
  The `:report` fallback prints bounded SBCL/CCL backtraces before unwinding;
  tests confirm real stack output. A live SBCL pseudo-terminal check entered
  the debugger, selected `RETURN-TO-APPLICATION-REPL`, evaluated another form,
  and stopped cleanly. CCL terminal debugger I/O remains unverified; its tests
  install hooks on the initial process because the adapter does not transfer
  dynamic bindings. Editor integration is explicitly deferred as recorded below.
- [x] Prototype SBCL Swank with `:communication-style nil` and `:fd-handler`,
  checking where forms actually execute. These are candidates requiring
  verification, not supported configurations yet.
- [x] Prototype CCL integration using its initial-process adapter.
- [x] Review the prototypes and select a demonstrated editor workflow or
  explicitly defer editor support. If none works reliably, use the documented
  terminal REPL workflow as the release criterion and keep editor REPLs
  unsupported.
- [x] If scheduling is needed, define execution order, returned values, condition
  delivery, shutdown, and GUI-thread reentrancy before implementing it.

  Not applicable to the selected terminal workflow: editor scheduling is deferred
  with editor support. These contracts remain prerequisites for future work;
  no scheduler has been implemented or advertised.
- [x] Implement and verify the selected editor adapter if a prototype succeeds.

  Not applicable after explicit deferral: no editor workflow or adapter was
  selected. The thread probes below are not successful end-to-end editor
  prototypes and do not establish editor support.
- [x] Retain and prominently document one GUI session per process for the first
  release.
- [x] Retain and prominently document the current rule that handlers for
  destroyed controls remain rooted until unbound or session cleanup.
- [x] Demonstrate explicit unbinding before destruction and verify that repeated
  bind/unbind cycles release registered closures.
- [x] Add a live-development example that updates controls and redefines callback
  functions within one session.

Verified locally on 2026-10-28: both pinned SBCL/CCL suites pass 21 scenarios.
The new `examples/live.lisp` is exercised by `tests/live.lisp`: text updates,
callback redefinition, 50 bind/unbind cycles with registry-release checks,
explicit unbinding before destruction, and frame recreation within one session.
The smoke test also verifies handler retention after native destruction and
release at session cleanup, plus rejection of a second application session.

Optional Swank (SLIME 2.32) loopback RPC probes passed for SBCL `nil`,
`:fd-handler`, and a `:spawn` rejection control, plus CCL `:spawn`. SBCL's first
two styles execute evaluation/body/callback on the launch thread. Its worker
control rejects GUI startup. CCL evaluates on a worker and runs the application
body/callback on the initial process without transferring caller dynamic
bindings. Reproduction commands and limits are in README.md. These experiments
cover one request containing one complete session, not persistent editing,
interruption, or editor debugger transport. SLIME/SLY support is explicitly
deferred; the terminal application REPL meets the first-release workflow gate.

**Acceptance:** The terminal workflow supports live updates, useful error
diagnosis, and recovery without losing the session. Debugger behavior and
lifecycle limits are documented and tested. Editor support is either demonstrated
or explicitly deferred.

## Phase 3: Validate supported platforms

- [ ] Execute macOS, Linux, and Windows hosted jobs and preserve results.
- [ ] Extend Windows checks to Lisp loading, callbacks, controls, and cleanup;
  mark complete only after the configured hosted Windows job passes.
- [ ] Check Unicode and REPL behavior on advertised combinations.

Windows CI now stages SHA256-pinned SBCL 2.6.9, places the bridge where the Lisp
loader expects it, exposes dependent wxWidgets DLLs, and runs native loading/ABI
preflight plus the full GUI suite. All hosted jobs retain per-process logs and
JSON summaries for 30 days with an `always()` artifact upload. Configuration is
not runtime evidence: the Windows setup and PowerShell installer are unexecuted
on this macOS host.

Local evidence recorded on 2026-10-28: both SBCL 2.6.9 and CCL 1.13 pass preflight
and 21 GUI scenarios, including Unicode and REPL tests. Logs and JSON summaries
are retained in dedicated ignored `evidence/platform/{sbcl,ccl}-20261028-phase3/`
directories, separate from the disposable dependency cache.
The reports identify the source commit and mark the working tree dirty because
they verify the uncommitted Phase 3 changes. Workflow lint and reporter checks
also pass. The downloaded Windows MSI's SHA256 matches the configured pin; it
was not executed locally.

**External dependencies:** this checkout has no Git remote. GitHub authentication
is available, but no repository association or hosted run URL has been provided.
There is no local Windows runtime, PowerShell, Linux display server, or container
runner. Hosted macOS/Linux/Windows execution and Unicode/REPL evidence on those
runners remain pending; Phase 3 is not complete. Commands and the distinction
between configured and validated platforms are documented in README.md.

**Acceptance:** Runtime evidence supports every advertised platform. Missing
repository access or runners remain explicit external dependencies.

## Phase 4: Complete a representative application

- [x] Extend an example using the existing controls and layouts.
- [x] Include handler replacement, explicit unbinding, and window recreation
  within the same session.
- [x] Identify concrete gaps exposed by the example and add controls or
  operations only where needed.

`examples/form.lisp` retains its ordinary load-and-run entry point. Its reusable
`examples/form-app.lisp` application combines labels, Unicode text input, a
checkbox, a style dropdown, and row/column layouts. `start-live` enters the
terminal application REPL; submit actions can be replaced or redefined by name.
Explicit close/reopen functions and a GUI reopen button unbind handlers before
destruction and recreate the window with retained input values. Both entry
points use only public bindings. The application exposed a need for reusable
example lifecycle functions, not new native controls or operations; the binding
API and ABI remain unchanged.

Verified locally on 2026-10-28 with pinned dependencies: SBCL 2.6.9 and CCL 1.13
each pass native preflight and all 23 GUI scenarios. `tests/form.lisp` adds normal
and live modes covering startup, controls, Unicode values, queued handler
replacement, named function redefinition, explicit unbinding, stale handles,
state retention, callback-driven recreation, and cleanup. An initial assertion
incorrectly expected deferred destruction to finish inside the recreation
callback; it was corrected to check after the appropriate event/cleanup boundary
and both modes pass. The six reporter tests also pass. Reports and logs are
retained separately from dependencies in `evidence/phase4/{sbcl,ccl}-20261028/`.

Accepted lifecycle limits remain visible in the README: one session per process,
and closures for user-destroyed windows remain rooted until session cleanup if
they were not explicitly unbound first. Manual input and visual layout have not
been verified. Phase 3's hosted-platform checks remain pending; these local
application results do not complete platform validation.

**Acceptance:** Ordinary startup and live development work through public APIs,
with the accepted lifecycle limits visible.

## Phase 5: Implement standalone packaging

- [x] Create a reproducible SBCL packaging example.
- [x] Resolve native libraries relative to the packaged application.
- [x] Stage required runtime libraries.
- [x] Test outside the checkout, from another working directory and without a
  development Quicklisp installation.

The completed example is scoped to macOS SBCL, locally verified on Apple Silicon
with SBCL 2.6.9. `packaging/build-form.py` saves the form and pinned Lisp
dependencies without initializing wxWidgets, copies the SBCL runtime, and stages
the bridge plus recursive non-system dylibs. It rewrites native paths relative
to each image, audits their resolution, and ad-hoc signs the copied images.
This host's package contains 10 native objects, including the runtime, wxWidgets,
its image dependencies, and zstd. Native inputs/hashes are recorded in a manifest;
the recipe is repeatable, not a byte-identical or fully native-version-locked
build. Linux/Windows packaging is not implemented, and Intel macOS is unvalidated.

The public explicit-path native loader no longer consults ASDF. The delivered
core deliberately has no registered wxlambda ASDF system or Quicklisp package,
and resolves its bridge beside the packaged runtime. A launcher supports normal
startup, `--repl`, and `--self-test`. Interactive mode restores SBCL's debugger
after the non-interactive image build.

Verified locally on 2026-10-28: build and five packaging checks pass. The verifier
copies the package outside the checkout to a path with spaces, uses an unrelated
working directory and empty HOME/SBCL_HOME, and denies checkout, Quicklisp,
compiled-cache, and Homebrew reads. A denied-read control proves the sandbox is
active. Packaged Unicode callbacks, window recreation, cleanup, and REPL
startup/recreation/exit pass. Withholding the staged bridge or wxWidgets core
produces the expected explicit load error and exit 1; those expected failures
are not application passes or unexpected failed checks. Evidence is retained in
`evidence/packaging/macos-sbcl-isolated/`. Both regular pinned SBCL/CCL suites
also pass preflight and 23 GUI scenarios after the loader change, with reports
in `evidence/phase5/{sbcl,ccl}-20261028/`.

The result is a command-line directory package, not a notarized Finder `.app`.
Other machines/OS versions and manual UI input remain unvalidated. Phase 3's
hosted checks and Phase 7's release preparation remain open; this local sample
does not establish release-wide platform support.

**Acceptance:** The sample launches, processes events, and exits without relying
on development paths or Quicklisp.

## Phase 6: Create the full wxWidgets coverage plan

- [x] Inventory the pinned wxWidgets API, including widgets, functions, overloads,
  events, styles, optional components and platform-specific declarations.
- [x] Mark implemented and partial operations in the detailed lists and class
  index, preserving their scopes and Lisp/native mappings across regeneration.
- [x] Group the remaining inventory into coverage implementation phases with
  dependencies, verification requirements and completion rules.
- [x] Preserve release preparation as Phase 7 and link the new coverage plan.

The [wxWidgets coverage plan](docs/WXWIDGETS-COVERAGE.md) records the implemented
baseline and coverage phases C1–C7. Its class index and per-header declaration
checklists cover all 398 wxWidgets 3.2.12 interface headers, with source links
and a hash manifest. A persistent status ledger retains checked/partial entries;
compiler scaffolding is context rather than binding tasks. These are
source-inventory entries, not a count of distinct
functions or a claim of full binding coverage. Final source/documentation and
inherited-API reconciliation is an explicit remaining coverage task.

**Acceptance:** Every pinned interface header has phased, source-linked work
items; existing support is distinguished from complete class/API coverage.
The plan is complete; implementing its unchecked bindings remains future work.

## Phase 7: Prepare the release

The docset build prerequisite was established before the first coverage slice.
Continue using it for subsequent implementation work, including Phase 3 fixes
and C1–C7 bindings, so required docset updates can be built and verified.

- [x] Establish a reproducible Cadre-format docset build and update workflow for
  implementation changes, with documented build/check/install commands. Generate
  format version 1 in `docs/docset/` with `manifest.sexp`, `index.sexp` and
  `entries.sexp`, docset ID `wxlambda`, and package `WXLAMBDA`.
- [ ] Review exports, documentation, examples, ABI compatibility, and limitations.
- [ ] Run supported-platform and packaged-application checks.
- [ ] Align ASDF and CMake release versions.
- [ ] Prepare release notes and installation, development, debugging, and
  packaging instructions.
- [x] Produce the docset containing an indexed reference for every implemented
  public wxlambda export, project guides/limitations, and example sources with
  explanations and run commands, all accessible through its index and contents.
- [ ] Require zero problems from `docset:check-docset`; verify public-export
  coverage and links, run the included examples, and verify Cadre discovery,
  symbol lookup and navigation to guides/examples using documented installation.

**Acceptance:** A new user can follow the documented installation-to-packaging
workflow. Documentation and runnable examples match the implemented interfaces;
the built docset uses Cadre's format version 1 and includes the public wxlambda
reference, project guides and example sources together. All three files are
present; `docset:check-docset` reports zero problems; export coverage, links,
example execution and Cadre discovery/lookup/navigation checks pass.

## Coverage implementation: first slice

- [x] Establish and verify the Cadre-format docset build before expanding bindings.
- [x] Complete `wxWindow::Show(bool)`, `IsShown()`, `Enable(bool)` and `IsEnabled()`
  through the native bridge, FFI and public Lisp exports, preserving setter results
  and recording the equivalent `Hide()`/`Disable()` mappings.
- [x] Add runnable window-state callbacks, focused tests and documentation, and
  update the persistent coverage ledger.

This bounded C2 slice builds on existing window handles; the C1 foundation
inventory and the rest of C2–C7 remain open. `show-window` retains its window
primary value and adds an optional show/hide argument and changed-state second
value. `shown-p`, `enable-window` and `enabled-p` expose own visibility and
effective input state. Native ABI 8 distinguishes false from native errors.

The initial [docset workflow](docs/DOCSET.md) delivered three committed files
containing the then-current 53 public exports, project guides and six example
sources with usage notes. Later slices extend that artifact.
It requires Python 3, SBCL and the shared `docset` library; no GUI is needed to
build/read the docs. Its checker, shared-reader discovery/search/lookup and
indexed guide/example reading pass, including an isolated installation.
The graphical Cadre viewer has not been manually exercised.

The [window-state test](tests/window-state.lisp) runs the actual example callbacks,
checks repeated setters, returned state-change flags, ancestor behavior and
invalid/stale session use. Both local Apple Silicon macOS suites pass native
preflight and all 24 GUI scenarios with wxWidgets 3.2.12, SBCL 2.6.9 and CCL 1.13;
reports are retained in `evidence/coverage-first/{sbcl,ccl}-20261028/`.
Reproduction commands are in [the window-state guide](docs/WINDOW-STATE.md). This does not
close Phase 3 hosted checks or Phase 7 release-wide acceptance.

## Coverage implementation: window metadata and CLOS API

- [x] Add `window-name`, `(setf window-name)`, `frame-title` and `(setf frame-title)`
  with native/FFI mappings and length-aware UTF-8 string ownership.
- [x] Add a runnable callback example, a user guide and public API reference
  records; track full name support and frame-only title support in the ledger.
- [x] Fix multiline preprocessor handling in the inventory generator so following
  classes and methods remain individually visible, with a regression check.
- [x] Establish 40 widget-facing CLOS generic functions and export all eight
  existing wrapper classes, preserving valid-call signatures and results.
- [x] Add portable CLOS tests and an example of wrapper subclassing/method hooks;
  record the CLOS implementation rule and update the reference generator.

This C2 slice supports logical names on all seven bound widget types, including
empty/Unicode/embedded-NUL names. Frame titles are exposed only for `wxFrame`;
other `wxTopLevelWindow` receivers and the broader C1/C2 inventories stay open.
Getter strings remain valid after later queries and destruction; setters copy
inputs. Native ABI 9 adds explicit byte lengths without changing earlier bridge
string-buffer contracts.

The [metadata guide](docs/WINDOW-METADATA.md) documents usage and focused checks.
At that checkpoint the docset covered 65 public exports, including eight
wrapper classes and 40 named generic functions, plus guides and seven example sources. The user
explicitly requested establishing CLOS before further widget-family expansion;
keyword-only factories, lifecycle functions, immutable event snapshots and the
FFI retain their existing roles. Native virtual overrides remain future work.
Both local macOS SBCL/CCL suites pass native preflight and all 26 GUI scenarios,
including the CLOS and metadata scenarios. Reports are retained in
`evidence/coverage-clos/{sbcl,ccl}-20261028/`. The earlier 25-scenario metadata
reports in `evidence/coverage-metadata/` predate the CLOS migration.

The macOS SBCL standalone form was rebuilt as `dist/wxlambda-form-clos` and
passes all five isolation/relocation/REPL/missing-library checks, with evidence
in `evidence/packaging/coverage-clos-20261028/`. The docset checker reports zero
problems; actual-package export coverage passes on both Lisps. Python and
inventory regressions cover method-signature rendering, stale-docset detection,
agent-content exclusion and multiline macro parsing. Graphical Cadre and hosted
platform verification remain release work; shared-reader checks are separate
from graphical-viewer testing.

## CLOS construction and properties: revision 3

- [x] Make `make-instance` construct every concrete native widget from initargs,
  with ordinary subclass defaults/slots and native-backed AFTER initialization.
- [x] Make factories thin MAKE-INSTANCE wrappers with validated `:class` and
  forwarded subclass initargs, and reject abstract/invalid construction clearly.
- [x] Roll back native allocations and callback roots on managed initializer
  failure, preserving the original condition or nonlocal exit.
- [x] Establish eight canonical generic SETF writers and migrate examples,
  tests, REPL scripts, packaging and documentation to them.

The user's later ground-up direction explicitly removes backward compatibility
as a requirement. It supersedes the earlier factory/CHANGE-CLASS construction
convention and the review's request to retain setter aliases. Six separate
setter aliases and receiver-compatibility fallback methods were removed;
unsupported receiver types now use CLOS dispatch errors. The native ABI remains
9; native handle/type and callback/lifetime checks are retained.

The public API has 59 exported symbols, including eight classes, 34 named widget
generic functions and eight generic SETF functions using the getter symbols.
Class docset pages include Construction sections; mutable-property pages include
SETF syntax and indexed compound writer names. The current docset has 73 entries
covering the API, guides and seven executable example sources.

Locally verified on Apple Silicon macOS with SBCL 2.6.9 and CCL 1.13: native
preflight and all 26 GUI scenarios pass, including direct construction of all
seven native kinds, subclass defaults/slots, factory class validation, initializer
rollback, property writers, notification recovery and method combination.
The initial SBCL run caught an obsolete native-error expectation for a wrong
receiver in the events test; the expectation now checks CLOS dispatch failure,
and the full rerun passes. Both reports are retained under
`evidence/clos-construction/` (`sbcl-rerun-20261028`, `ccl-20261028`); the original
failed `sbcl-20261028` report remains intact.

The rebuilt macOS SBCL form in `dist/wxlambda-form-native-clos` passes all five
packaging checks, recorded in `evidence/packaging/native-clos-construction-20261028/`.
Docset validation on both Lisps verifies all 59 exports and the construction and
SETF sections; Python/inventory regressions pass. Hosted-platform checks and the
Cadre graphical viewer remain unvalidated.

## Property accessor names: revision 4

- [x] Rename the text readers and generic SETF writers to `text` and
  `label-text`, with no aliases for the former reader names.
- [x] Update callers, embedded REPL/package scripts, examples, API reference
  records, docset indexes and persistent coverage mappings.
- [x] Record property-style naming in the project rules and add regression
  assertions that obsolete readers/writers are absent.

The CLOS construction and SETF protocol is unchanged. Public export count remains
59 and the docset remains 73 entries; the property names and index IDs now use
natural accessor names. The private CFFI/native getter names remain internal
implementation details, so no native ABI change is needed.

Both local macOS SBCL/CCL suites pass preflight and all 26 scenarios, with
reports in `evidence/property-names/{sbcl,ccl}-20261028/`. The rebuilt standalone
form passes five packaging checks in `evidence/packaging/property-names-20261028/`.
The Python/inventory checks and both actual-package docset checks pass, including
indexed SETF forms under the new names. Hosted platforms and the graphical
Cadre viewer remain unvalidated.

## Decisions

- D1 Target a bounded first release while tracking full wxWidgets coverage in
  the Phase 6 plan; rejected: making exhaustive bindings a release requirement.
- D2 Gate editor support on prototype evidence because main-thread integration
  remains unproven; rejected: making release completion depend indefinitely on
  SLIME/SLY.
- D3 Accept both existing lifecycle limits for the first release because restart
  support and destruction notifications require separate lifecycle designs;
  rejected: silently expanding their scope.
- D4 Pause GUI processing during debugging because nested event dispatch adds
  reentrancy risks; rejected: implicitly pumping events inside the debugger.
- D5 Default the application REPL to the host debugger and require explicit
  `:on-error :report` for automatic recovery because interactive diagnosis needs
  live evaluation restarts; rejected: silently discarding restarts by default.
- D6 Defer editor integration and scheduling for the first release because thread
  probes do not establish a persistent editor/debugger workflow; rejected:
  advertising SLIME/SLY support from single-request thread checks.
- D7 Ship the packaging example as a separate SBCL runtime/core with sibling
  native libraries because relocation can patch/sign the runtime without
  modifying an appended Lisp core; rejected: a single Mach-O/core executable
  for this first packaging example.

- D8 Use public CLOS wrapper classes and generic widget operations because the
  user requested Lisp dispatch and specialization before further expansion;
  rejected: continuing ordinary functions with native-only type dispatch.

- D9 Use native-backed INITIALIZE-INSTANCE construction and canonical SETF
  properties because the user requested a ground-up CLOS API without backward
  compatibility; rejected: factory-then-CHANGE-CLASS construction and legacy
  setter/receiver compatibility scaffolding.

- D10 Name the mutable properties `text` and `label-text` because the same
  names read naturally for reads and SETF writes; rejected: get-prefixed readers
  and compatibility aliases during ground-up API development.

Implement these as separate reviewable changes, beginning with baseline
corrections and REPL regression tests. Platform work can proceed alongside local
development when runners become available.
