# Implementation plan

The final goal is complete wxWidgets coverage across C1–C7, including supported
platforms, optional components and documented public interfaces. A usable first
release with existing controls, reliable REPL-driven development and standalone
packaging is an intermediate milestone. Phase 6 inventories the full scope;
partial C1/C2 implementation is underway and does not complete that goal.
This is a plan; unchecked steps are not claims of implemented or verified
behavior.

## Session handoff: full coverage work

The user has confirmed that **complete wxWidgets coverage is the final goal**.
Keep every required missing/partial API open; a bounded release, working example,
local test result or explicitly unsupported decision cannot close the final goal.
Use the [coverage overview](docs/WXWIDGETS-COVERAGE.md), generated
[phase/header backlog](docs/coverage/README.md), class index and exact C1–C7
declaration checklists together. The pinned inventory is wxWidgets 3.2.12; C7
must reconcile additional documented public interfaces beyond those headers.

### Completed implementation checkpoints

- `6f6312e`: progress gauges, generic SETF properties, subclass example and
  pulse-to-determinate regression; independent review finding resolved.
- `ed3ca87`: C1 application state and pending-event dispatch, callback/pump/
  cleanup reentrancy guards and fresh-load inactive-session regression;
  independent review finding resolved.
- Event-handler ownership milestone: native-backed standalone handlers, borrowed
  window base identities, liveness and handler-enabled SETF state, plus weak native
  window invalidation. See the latest commit and the C1 handler section below.
- Command-observer binding milestone: independent registrations, ID/range filters,
  copied standalone queues, self/peer unbinding and deferred callback-time disposal.
  Four command kinds only; see the latest commit and the binding section below.
- Scoped-event milestone: optional current-callback contexts control native Skip
  and propagation, with nested-callback isolation and copied native type IDs.
  Four command kinds only; veto-capable classes and broader families remain open.
- Close-family milestone: :close snapshots/contexts, CanVeto-guarded SETF veto,
  synthetic close queues and frame close requests; acceptance is not destruction.
  `event-type` replaces the command-only lookup without an alias.
- Registered-notify milestone: session keyword declarations for custom command/
  notify types, copied text/integer payloads, scoped Veto/Allow and synchronous
  dispatch with native handled/final-veto results. Session routing remains open.
- Borrowed application milestone: cached application-handler view with inherited
  bindings, dispatch and enabled state; no native ownership transfer or construction.
- Session-event milestone: query/end-session tags and scoped logging-off metadata,
  native application bindings and isolated owned-handler synthetic probes.
  Real OS session delivery remains unverified.
- Metadata milestone: copied/current native timestamps and command classification,
  timestamp SETF with native LONG bounds and close-family CanVeto SETF.
- Routing milestone: current event-ID reads/SETF, signed32 including native -1,
  and parent exact/range/wildcard matching after mutation.
- Origin/type milestone: current native type reads and captured/current opaque
  origin IDs for registered bridge objects; no unchecked pointer/class access.
- Category milestone: saved/current virtual category flags and compiled native
  keyword mask lookup; selective YieldFor remains open.
- Tag-mutation milestone: registered same-family native type SETF with actual
  native class checks; built-in/session tags remain excluded.
- Current bridge ABI: 23. Public API: 104 exports, nine window wrapper classes
  (eight concrete widget kinds) plus the event-handler base, 62 named generics and
  eighteen generic SETF writers. The application-handler is a borrowed view; no
  native application construction or arbitrary-event class binding exists yet; built-ins plus registered command/notify names have
  observer/contextual bindings.
- Latest implementation verification: 40 GUI scenarios and native preflight
  pass on local Apple Silicon macOS SBCL 2.6.9 and CCL 1.13. Final reports:
  `evidence/event-tags/{sbcl,ccl}-20261028/`. Five package checks
  pass in `evidence/packaging/event-tags-20261028/`.
  Hosted Linux/Windows GUI behavior and the graphical Cadre viewer remain open.

### Next dependency and working order

1. Continue **C1**, before adding further C2 widget families. Application dispatch
   boundaries are established only for the bridge-owned session.
2. Reuse the implemented event-handler ownership/state base: standalone objects
   are session-owned; windows borrow their existing native identity, with weak
   tracking independent of destroy-event delivery. There is no pushed-handler
   ownership; standalone dispatch supports command observers and the non-command close family.
   Preserve native-backed subclass
   initargs, defaults, AFTER hooks and mixed-resource rollback.
3. Generic binding/unbinding supports command and close kinds with ID filters. Optional
   borrowed contexts now control Skip/propagation in the innermost callback; saved
   snapshots carry process-local native type IDs. Close veto and frame request
   policy are implemented. Registered command/notify types and synchronous dispatch are implemented.
   The borrowed application handler view and query/end-session metadata/bindings
   are implemented, as are scoped CanVeto mutation, timestamp and command metadata.
   Scoped native ID mutation and routing semantics are implemented. Next define
   origin retention/native ownership contracts. Bounded registered-tag mutation
   with actual native class checks is implemented. Category metadata is implemented. Current type
   reads and bounded origin identities are implemented; arbitrary native tag/origin
   writers and general wxObject wrappers remain open. Native OS session delivery is unverified. Native session events are
   wxApp-only, and default END_SESSION handling can exit the process: never
   synthesize live application end-session events as an ordinary test. Use owned
   handlers for synthetic family tests and distinguish OS validation from them.
   Standalone native TryAfter must suppress app fallback for session kinds;
   ordinary standalone wxEvtHandler dispatch can otherwise reach app defaults.
   Native control-specific notify subclasses and remaining event ownership stay open. Preserve immutable
   snapshots and opaque binding/context identities; never expose a native event
   pointer after callback return. Arbitrary native event classes/userData, session routing and
   linked/pushed handlers remain gaps.
4. Build timer/idle, event propagation/skip/veto and worker-to-GUI dispatch on
   those contracts, adding prerequisite value types before their consumers.
   Existing immutable event snapshots do not provide mutable native event access
   or native virtual overrides. Do not mark those capabilities complete.
5. Continue all remaining C1 groups, then C2–C7, reviewing dependencies and
   supported platform/build variants rather than treating the current slice
   ordering as an exhaustive design for the rest of the project.

### Resume and verification checklist

- Inspect `git status` and the latest log first; preserve unrelated changes.
  Read `AGENTS.md`, this handoff and the relevant coverage declarations.
- Use the required quiet-command wrapper for builds, tests and batch commands.
- Treat `docs/coverage/status.json` as the source of status truth. Update exact
  header/line/signature mappings and scopes, then regenerate all checklists,
  the class index and backlog. Never hand-edit generated marks or totals.
- Obtain the pinned upstream `interface/` tree if the previous temporary cache
  is gone; verify its header hashes against `docs/coverage/manifest.json`.
  `../docset` (or `WXLAMBDA_DOCSET_SOURCE`) supplies the shared docset reader;
  `.ci-deps/` contains the pinned test dependencies, fetched through the CI tool.
- Reproduction examples (replace the source placeholder):

```sh
python3 tools/coverage_inventory.py /path/to/wxWidgets-3.2.12/interface
python3 tests/coverage-inventory.py /path/to/wxWidgets-3.2.12/interface
python3 -m unittest discover -s tests -p 'test_*.py'
python3 tools/docset.py build
python3 tools/docset.py check
```

- Every implementation slice needs matching reference/guides, runnable example
  sources, docset updates and relevant behavior/error/lifetime tests. Run affected
  examples; verify actual-package export and SETF coverage with
  `tests/docset.lisp` on both Lisps, plus shared-reader discovery and guide/example
  access. The generated coverage summary is also included in the docset.
- Follow the established workflow: finish a bounded dependency-building slice,
  obtain independent review, resolve findings, rerun affected checks, regenerate
  docs, commit, and record the result and next dependency here. Keep evidence
  directories fresh; the GUI runner rejects reused report directories.
- Distinguish local checks from hosted platform and graphical-viewer validation.
  For documentation-only changes, verify regeneration/links/docset; do not claim
  GUI suites were rerun. Only close a phase when all its required scope and
  verification are complete.
- For isolated docset installation checks, use a dedicated discovery parent such
  as `/path/to/isolated-docsets/wxlambda`, with only one copy of this docset ID.
  The reader deduplicates IDs across siblings; old test copies under a shared
  temporary parent can otherwise select a previous artifact. A shared-parent
  install check exposed that collision during the coverage-summary update;
  repeat with a dedicated root rather than weakening content comparisons.

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
The inventory-plan deliverable is complete. C1/C2 implementation is underway;
all remaining pending/partial scope across C1–C7 remains required for the final goal.

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

## Coverage implementation: progress gauges

- [x] Add native-backed `gauge` construction and the thin `make-gauge` factory,
  including subclass defaults, slot initargs and native access in AFTER hooks.
- [x] Add generic `gauge-value`/`gauge-range` readers and SETF writers,
  `gauge-vertical-p` and `pulse-gauge`, with checked native handles and arguments.
- [x] Add a runnable subclass/callback example, focused GUI scenario, guide,
  reference entries and persistent declaration/class/style coverage mappings.
- [x] Prevent guide/example stem aliases from shadowing public symbols in Cadre
  lookup, with a regression test and stricter build/check lookup assertions.

This bounded C2 slice exposes `wxGauge` progress, positive ranges, horizontal or
vertical construction and indeterminate pulsing. Range assignment clamps the
current native value and restores determinate mode. Pulse animation/value
readings remain platform-dependent. Custom constructor IDs, geometry, validators,
names and additional styles, two-step creation and the remaining inherited API
stay open. Native ABI 10 requires rebuilding the bridge with these bindings.
The API now has 65 exports, nine wrapper classes, 39 named generic functions and
ten generic SETF writers. The docset contains 81 entries across API, guides and
eight example sources; installation instructions remain in [DOCSET.md](docs/DOCSET.md).

Local Apple Silicon macOS verification passes native preflight and all 27 GUI
scenarios on SBCL 2.6.9 and CCL 1.13. Evidence is retained in
`evidence/gauge/sbcl-20261028.log` and `evidence/gauge/ccl-20261028/`; a focused
rerun on both Lisps also passes after the final helper refactor. The sandboxed
initial SBCL gauge process stalled on macOS window-service access and was
terminated; GUI verification passed with window-service access outside the
agent sandbox. Reporter/inventory tests initially caught obsolete scenario and
class counts, which were updated and pass on rerun. Actual-package docset
validation caught colliding guide/example aliases for GAUGE; the alias fix and
regression now pass, with zero `docset:check-docset` problems, complete export
and SETF lookup coverage, links, discovery and indexed guide/example access on
both Lisps. An isolated install under a temporary docset root passes discovery
and indexed reading. Python and pinned-upstream inventory regressions pass.

The rebuilt `dist/wxlambda-form-gauge` passes all five packaging isolation,
relocation, REPL and missing-library checks, with reports in
`evidence/packaging/gauge-20261028/`. Hosted Linux/Windows GUI execution and the
graphical Cadre viewer remain unvalidated. This slice does not complete C1 or C2.
Independent static review found that the example resumed determinate progress
from platform-dependent pulse readings. The example now stores completed work
in Lisp and tests pulse-to-advance directly; the revised focused scenario passes
on both Lisps. The reviewer verified the fix and reported no remaining actionable
findings. The user has directed that unfinished C1 foundations take priority before further
C2 widget expansion; finish and review this gauge slice, then return to C1.

## C1 foundation: application event dispatch

- [x] Expose `main-loop-running-p`, `pending-events-p` and
  `process-pending-events` for the active bridge application, distinguishing wx
  pending events from GUI/OS polling and idle processing.
- [x] Guard RUN, both event pumps and native cleanup against callback/pump
  reentrancy, including synchronous notifying setters before RUN starts.
- [x] Add the runnable application-events example, focused regression scenario,
  guide, API reference and pinned C1 declaration mappings.

This is an application-dispatch prerequisite within C1, not completion of the
application or event-handler inventory. The next dependency is event-handler
ownership and generic binding, followed by timer/idle/propagation and
worker-to-GUI dispatch, with required value/ownership contracts before consumers.
All new APIs require an active main-thread session. Queries return T/NIL;
pending-only dispatch returns NIL and re-signals contained callback conditions
after returning from native code. Existing synchronous notifications and the
cooperative application REPL remain supported. Native ABI 11 adds these entry
points and the dispatch boundary; rebuild the bridge alongside the bindings.

Local Apple Silicon macOS verification passes native preflight and all 28 GUI
scenarios on SBCL 2.6.9 and CCL 1.13, including the example, loop/queue queries,
pending dispatch, raw/native callback guards, notifying-setter boundaries,
debugger pauses, inactive sessions and recovery after callback failure.
The initial focused test used an incorrect zero-argument text callback; it was
corrected to accept the event snapshot and both Lisp reruns pass. Full reports
are retained in `evidence/application-events/{sbcl,ccl}-20261028/`.
Independent review found that fresh-ASDF-load calls initially raised an FFI
lookup error instead of NATIVE-ERROR before bridge loading. All three new public
functions now check the active session before calling the FFI. The fresh-load
regression verifies NATIVE-ERROR without loading the bridge, then checks raw
native pre-session errors. The reviewer independently verified this fix and
reported no remaining actionable findings. Both full 28-scenario suites pass
after the fix; final reports are under
`evidence/application-events/{sbcl,ccl}-review-fix-20261028/`.

Python reporter/docset regressions and all pinned-upstream inventory checks pass.
The docset contains 86 entries and covers all 68 public exports, guides and nine
example sources. Build/check and actual-package export/SETF lookup verification
pass on both Lisps with zero `docset:check-docset` problems; indexed reading,
links, shared-reader discovery and an isolated temporary installation are
verified. Installation instructions are
in [DOCSET.md](docs/DOCSET.md).
The rebuilt `dist/wxlambda-form-application-events-reviewed` passes all five packaging
isolation, relocation, REPL and missing-library checks; evidence is in
`evidence/packaging/application-events-reviewed-20261028/`. Hosted Linux/Windows GUI
execution and the graphical Cadre viewer remain unvalidated. The C1 inventory
remains open, and no further C2 widget families were added.

## C1 foundation: event-handler ownership and state

- [x] Add native-backed `event-handler` construction, thin validated standalone
  factory, explicit standalone destruction, liveness and generic enabled state.
- [x] Make `window` inherit the event-handler interface while borrowing its existing
  native widget identity; retain a single allocation per window and managed CLOS
  subclass/default/initarg/AFTER semantics.
- [x] Generalize initializer rollback across standalone handlers, nested windows
  and callback roots; session cleanup frees remaining standalone handlers.
- [x] Track windows weakly so disabling handler event processing cannot leave
  dereferenceable dead pointers when destroy-event delivery is suppressed.
- [x] Add standalone/subclass and window callback examples, focused ownership/
  state/lifetime tests, API/guide/docset content and persistent C1 coverage.

Native ABI 12 adds standalone handlers and own event-processing state.
`handler-enabled-p` is distinct from window input `enabled-p`; native routing
and parent propagation rules still apply. Native lifetime observation returns
NIL for stale identities only within an active session. Borrowed window handlers
reject standalone destruction. Dead weak registry entries can remain until
session cleanup; they do not retain or dereference destroyed native windows.
Legacy control callback roots still require explicit unbinding before window
destruction or remain rooted until session cleanup. No generic binding, standalone
event queues, pushed-handler ownership or native virtual overrides are implemented.

Local Apple Silicon macOS native preflight and all 29 GUI scenarios pass on
SBCL 2.6.9 and CCL 1.13, with evidence in
`evidence/event-handlers/{sbcl,ccl}-20261028/`. Tests include the actual example
callbacks, disabled-window/child destruction, owned/borrowed misuse, direct/factory
class-object construction, native AFTER access, mixed-resource/callback-root
rollback, invalid construction, stale identities and inactive sessions. The
initial focused test mistakenly expected REINITIALIZE-INSTANCE to re-run native
construction; it was corrected to test repeated INITIALIZE-INSTANCE, matching the
existing construction contract. Both focused reruns and complete suites pass.

Independent static review found no actionable correctness issues. Its wording
nit about dead weak entries was corrected to state that entries can remain until
session cleanup. Python and pinned-upstream inventory regressions pass.
The generated docset contains 94 entries covering all 73 public exports, guides
and ten example sources. Build/check reports zero problems and verifies links,
shared-reader discovery and indexed guide/example access. Actual-package and
isolated-install checks accompany this slice; graphical Cadre and hosted
Linux/Windows GUI validation remain open.

The rebuilt `dist/wxlambda-form-event-handlers` passes all five packaging
isolation, relocation, REPL and missing-library checks, recorded in
`evidence/packaging/event-handlers-20261028/`. The next C1 slice is generic
multi-event/ID binding and safe unbinding, using the ownership base established
here. Full event-handler and C1 coverage remain open.

## C1 foundation: independent command-event bindings

- [x] Add immutable `event-binding` identities with `bind-event`, `unbind-event`
  and `binding-active-p`, using native Bind/Unbind with exact registration identity.
- [x] Support independent observers for four existing command-event kinds on
  standalone/window handlers, including signed command IDs and inclusive ranges.
- [x] Add copied synthetic `queue-event` payloads, immutable `event-id` snapshots
  and explicit UTF-8 payload lengths preserving embedded NUL text.
- [x] Keep active native callbacks alive across self/peer unbinding; defer
  callback-time receiver deletion through application CallAfter, with immediate
  target invalidation and root pruning at Lisp boundaries.
- [x] Make control-specific hooks observers too, preserving ordinary command
  routing to parent observers; add examples, API/guide and coverage/docset updates.

Native ABI 13 extends snapshots with command IDs and text byte lengths. Snapshot
`event-source` is the binding receiver, not necessarily the native originating
child; origin-object lookup remains unbound. All command callbacks automatically
Skip; mutable Skip/veto/propagation contexts, arbitrary native event types and
userData, pushed chains and worker dispatch remain open. Generic roots can remain
until a Lisp boundary during a blocking RUN; the control-specific explicit-unbind
root contract remains unchanged. Initializer rollback removes generic registrations
across nested resources. Owned/borrowed lifetime restrictions remain in force.

Initial full suites pass native preflight and all 30 scenarios on local macOS
SBCL 2.6.9 and CCL 1.13, in `evidence/event-bindings/{sbcl,ccl}-20261028/`.
Independent review confirmed a propagation issue: dormant control-specific hooks
consumed commands before parent generic observers. All four hooks now Skip,
including after convenience callback delivery. Final full reruns pass all 30
scenarios in `evidence/event-bindings/{sbcl,ccl}-observers-20261028/`, including
parent/child receiver/ID checks and self-disposal in synchronous notifying text,
checkbox and choice setters. Additional focused checks cover disabled-receiver
expiration and active-registration state while event processing is disabled.

Review verified exact binding lifetime, self/peer unbinding, deferred receiver
disposal, snapshot copying, rollback and the final documentation; no actionable
findings remain. An initial Lisp compile caught an extra parenthesis, which was
corrected; the first pre-session test incorrectly supplied a symbol to
ALLOCATE-INSTANCE instead of a class object, also corrected before passing reruns.
Python and pinned-upstream inventory regressions pass. The docset has 103 entries
covering all 80 exports, project guides and eleven example sources, with zero
checker problems. Actual-package/SETF lookup and isolated install checks verify
shared-reader discovery and guide/example access. Graphical Cadre and hosted
Linux/Windows GUI behavior remain unverified.

The final `dist/wxlambda-form-event-bindings-reviewed` passes all five package
isolation, relocation, REPL and missing-library checks, recorded in
`evidence/packaging/event-bindings-reviewed-20261028/`. Next establish native
event class/type representation and separate scoped mutable event contexts before
dependent event families. This does not complete wxEvtHandler or C1 coverage.

## C1 foundation: scoped native event contexts

- [x] Add `event-context` values for `bind-event :with-context t`; contextual
  callbacks receive `(snapshot context)` and explicitly control native Skip.
- [x] Add generic `event-skipped-p` with canonical SETF, `event-propagates-p`,
  stop/resume propagation actions and `event-context-active-p`.
- [x] Guard native event borrowing with monotonically allocated tokens, native
  callback depth and an RAII scope stack. Nested contextual, observer and
  control-specific callbacks cannot use a suspended outer context; it becomes
  usable again when they return. Expired readers/writers reject native access.
- [x] Copy native event type integers into immutable snapshots, with lookup for
  four existing command tags; add guides, runnable example and C1 coverage.

Native ABI 14 extends snapshot metadata and the binding mode. Observer mode
keeps automatic Skip; contextual mode starts unskipped and consumes unless the
callback sets Skip true. Propagation level is separate from Skip. Unbinding or
deferred disposal does not invalidate a context before its current callback
returns; errors expire it on return without rolling back native mutations.
Snapshot and context tokens remain immutable. Native type IDs are process-local
metadata, not stable serialized identifiers or arbitrary type registration.

Local Apple Silicon macOS native preflight and all 31 GUI scenarios pass on
SBCL 2.6.9 and CCL 1.13, with reports in
`evidence/event-contexts/{sbcl,ccl}-20261028/`. Focused checks also pass on both
Lisps for native type copying in all four control-specific callback modes and
outer-context restoration after a handled nested contextual error. Tests cover
consumption/forwarding, propagation stop/restore/zero, nested suspension, stale
readers/writers, callback conditions, self-unbinding and deferred receiver disposal.
The first compile caught an extra closing parenthesis in the extended native
binding call; it was corrected before the passing focused and full-suite runs.

Independent runtime and documentation review found no actionable correctness
issues. A docstring was clarified to distinguish contextual consumption from
observer mode. Python and pinned-upstream inventory regressions pass. The docset
contains 114 entries covering all 89 exports, guides and twelve example sources,
with zero checker problems. Actual-package/SETF lookup, links, shared-reader
discovery and isolated-install guide/example checks accompany this slice.
Hosted Linux/Windows GUI and the graphical Cadre viewer remain unverified.

The rebuilt `dist/wxlambda-form-event-contexts` passes all five packaging
isolation, relocation, REPL and missing-library checks, recorded in
`evidence/packaging/event-contexts-20261028/`. Full wxEvent/C1 coverage remains open:
veto-capable classes, arbitrary event-family/type registration, native event
construction/cloning, origin/ID/type mutation, userData, linked/pushed handlers,
timer/idle and worker dispatch remain future work. The next dependency is broader
event-family representation and veto-capable event classes with checked borrowed
lifetimes, before dependent event families.

## C1 foundation: close-event family and checked veto

- [x] Generalize native binding dispatch to the wxEvent base with checked family
  payload extraction; add `:close` / `wxEVT_CLOSE_WINDOW` / `wxCloseEvent`.
- [x] Add immutable `event-family` and close-flag snapshot reads, plus current
  context `event-can-veto-p` and generic SETF `event-vetoed-p`, guarding actual
  class and native CanVeto before setting or clearing veto.
- [x] Add copied synthetic close events with an initial `:can-veto-p` option and
  a frame-only synchronous `close-window` request preserving native acceptance.
- [x] Rename the command-only type lookup to `event-type`, without an alias,
  and migrate examples, tests, references and persistent coverage mappings.
- [x] Add the veto-policy example and close-family tests, guides and docset.

Native ABI 15 extends snapshots with close flags. Snapshot values are copied
before each callback and stay immutable; live veto access is limited to the
current close context. Non-close contexts and non-vetoable close requests reject
veto writes before wx assertions. Force disallows veto but does not bypass a
consuming contextual handler. Acceptance is not a destruction guarantee. A veto
handler should consume the event rather than delegate to default closing.
Queued close events on frames can invoke native default closing. Observer errors
do not implicitly veto; their automatic Skip can still allow default closing.

Native preflight and all 32 local Apple Silicon macOS GUI scenarios pass on
SBCL 2.6.9 and CCL 1.13. Reports are in
`evidence/close-events/{sbcl,ccl}-20261028/`. Focused checks also pass on both Lisps
for immutable snapshot SETF rejection and observer-error/default-close behavior.
Tests distinguish real veto, consumption, force and default deletion; cover
synthetic CanVeto flags, wrong-family/expired access and handled nested close
errors while RUN continues. A snapshot-extension compile warning exposed a
leftover variable in the edited constructor call; it was corrected to explicit
boolean conversions before passing verification.

Independent runtime/documentation review found no actionable correctness issues.
The queue docstring and a stale four-keyword sentence were corrected for the
new family. Python and pinned-upstream inventory regressions pass. The docset
has 120 entries for all 93 public exports, guides and thirteen example sources,
with zero checker problems; actual-package/SETF lookup, links, shared-reader
discovery and isolated-install navigation checks accompany this slice.
The rebuilt `dist/wxlambda-form-close-events` passes all five package checks,
with evidence in `evidence/packaging/close-events-20261028/`.

Full wxCloseEvent/C1 support remains open: wxNotifyEvent, query/end-session tags,
logging-off state, arbitrary event tags/registration, persistent event cloning,
context capability mutation, userData, pushed handlers, timer/idle and worker
dispatch remain required work. `wxWindow::Close` is mapped only for wxFrame;
other receiver types are not complete. Hosted Linux/Windows GUI and graphical
Cadre validation remain unverified. Continue C1 notify/session/event registration
and ownership prerequisites before dependent event families or new widgets.

## Registered command/notify types and synchronous dispatch (2026-10-28)

Bridge ABI 16 adds session-scoped REGISTER-EVENT-TYPE declarations, copied
command/notify text and integer snapshots, current-context notify Veto/Allow,
and PROCESS-EVENT. Synchronous dispatch returns native handled-p and final
vetoed-p independently: skipped observers can run while handled-p is NIL.
Synthetic events remain owned through callbacks and deferred receiver disposal.
Registration metadata expires with the session; saved snapshots remain readable.
There is no unregister or rollback of a declaration after failed initialization.

Both local SBCL and CCL pass native preflight and all 33 GUI scenarios, including
the runnable notify example. Python and pinned inventory regressions pass.
The docset contains 125 entries covering all 96 exports, guides and fourteen
examples. Five relocated-package checks pass; evidence paths are in the handoff.
Independent runtime and documentation review found no runtime issues; stale
snapshot/type/veto reference descriptions identified in review were corrected.

Native session routing is deliberately deferred: it requires a borrowed wxApp
handler view with defined lifetime, and default END_SESSION handling can exit
the process. Complete that prerequisite before query/end-session and logging-off
support. Derived notify classes, broader event ownership and all remaining C1–C7
backlog items remain open. Hosted Windows/Linux GUI and graphical Cadre checks
remain unverified.

## Borrowed application handler prerequisite (2026-10-28)

Bridge ABI 17 adds APPLICATION-HANDLER and CURRENT-APPLICATION-HANDLER as a
cached, borrowed view of the bridge-managed wxApp. It reuses inherited event
binding, queueing, synchronous processing, liveness and enabled-state generics.
Direct construction (including subclasses/factories) and explicit destruction
are rejected. Bindings are removed before native wxApp cleanup; saved snapshots
remain readable while saved views reject native operations outside the session.
Every lookup validates the native main-thread/session boundary, even when cached.

Actual-package docset validation caught a missing Construction section for the
borrowed class; review independently identified it, and the section was added
before repeating the checks. Independent static review found no runtime issue. Its documentation note was
applied: disabling the real application handler can delay application-queued
work, including deferred disposal, until restoration or cleanup. The example
and regression test cover identity, delivery, notify veto, enable/restore,
rejected construction/disposal, session cleanup and saved snapshots.

Local SBCL and CCL pass all 34 GUI scenarios plus native preflight. The Python
and pinned inventory tests pass; five relocated package checks pass. The docset
has 129 entries for all 98 exports, guides and fifteen examples; checker,
actual-package export/SETF coverage, isolated-install discovery and guide/example
navigation checks pass. Evidence paths are in the handoff. Hosted Linux/Windows
GUI and graphical Cadre validation remain unverified.

Continue C1 with session-family metadata and synthetic owned-handler tests before
validating OS application routing. Default END_SESSION may exit the process;
never emit it on the managed application as an ordinary regression test.
All required missing/partial C1–C7 APIs remain open in the coverage inventory.

## Query/end-session and logging-off metadata (2026-10-28)

ABI 18 adds :QUERY-END-SESSION and :END-SESSION to the close family, with
immutable logging-off snapshots and generic EVENT-LOGGING-OFF-P context SETF.
Native bindings accept the borrowed application view and owned standalone
handlers; window bindings reject. Synthetic queue/process accepts only owned
handlers. Internal StandaloneHandler::TryAfter blocks wxApp fallback for session
tags, including unmatched, skipped and disabled dispatch. Upstream source
inspection showed that plain wxEvtHandler otherwise forwards unhandled events
to wxApp; native default END_SESSION can exit the process. Other kinds retain
normal dispatch. Native application bindings are tested without emitting events.

Focused example/regressions and all 35 GUI scenarios plus preflight pass on local
SBCL and CCL. Python and pinned inventory regressions pass. Five relocated
package checks pass; evidence paths are in the handoff. The 132-entry docset
covers all 99 exports, guides and sixteen examples, with zero checker problems
and actual-package export/SETF coverage, discovery, isolated installation and
API/guide/example lookup checks on both Lisps.

Independent review identified missing reserved-kind snapshot decoding while the
slice was being assembled; the decoder was corrected before verification.
Review then found no actionable runtime issue. Documentation distinguishes
synthetic CanVeto defaults from native flags; changing logging-off metadata does
not change OS intent. Real OS session delivery/policy, hosted Windows/Linux GUI
and graphical Cadre validation remain unverified. All native session-event
coverage remains partial. Next complete scoped CanVeto mutation and remaining
native snapshot metadata, then event ownership/family prerequisites in C1.

## Scoped CanVeto and native event metadata (2026-10-28)

ABI 19 adds EVENT-TIMESTAMP and EVENT-COMMAND-P for saved snapshots and current
contexts. Timestamp SETF accepts the signed native C LONG range, with Lisp and
native checks before narrowing the fixed int64 transport. Native status/output
separation preserves negative timestamps, including -1. Synthetic timestamps
start at zero; no timestamp-construction option or scheduling behavior is added.
Command classification copies native IsCommandEvent, including notify=true.

SETF EVENT-CAN-VETO-P maps current wxCloseEvent::SetCanVeto directly. False can
be re-enabled; native GetVeto hides a stored veto while CanVeto is false and
reveals it if re-enabled. Every veto write checks the current flag. Notify
capability remains intrinsic/read-only, and snapshots have no property writers.
Explicit CanVeto mutation can affect forced native close results, but cannot
guarantee OS veto acceptance. Updated guides remove the prior permanent-false
forced-close assumption. Native context lifetime/isolation remains enforced.

Independent runtime review found no actionable issue. The runnable example and
focused regression pass both Lisps; all 36 local GUI scenarios and preflight
pass on SBCL and CCL. Python and pinned inventory regressions pass. Five rebuilt
relocated-package checks pass, with evidence paths in the handoff. The docset
has 136 entries for all 101 exports, guides and seventeen examples; checker,
actual export/SETF coverage, isolated-install discovery and API/guide/example
access checks pass on both Lisps. No implementation test failures were observed.
Hosted Windows/Linux GUI, actual OS session routing and graphical Cadre remain
unverified; do not claim full native event class coverage. Continue C1 with scoped
ID mutation/routing semantics, then native type/origin and ownership prerequisites.

## Scoped event-ID mutation and routing (2026-10-28)

ABI 20 extends EVENT-ID to a generic saved/current reader and context-only SETF.
Signed32 bounds apply; native SetId permits -1 as literal metadata, while binding
filters and synthetic initial IDs keep their existing -1 restriction. Status/output
separation preserves negative IDs. Snapshot construction uses a private identifier
slot; no mutable snapshot accessor is introduced.

Native ID changes affect later exact/range matching without changing windows,
filters or earlier snapshots, restarting dispatch or promising local callback
order. Parent-routing tests establish order structurally and verify matching,
Skip/consumption, wildcard behavior, queued delivery, signed bounds and current/
nested/expired context isolation. The standalone example verifies original and
forwarded snapshots and releases its resources.

Independent review found no remaining actionable issues. Focused example/tests
and all 37 GUI scenarios plus preflight pass locally on SBCL/CCL. Python and
pinned inventory regressions and five rebuilt relocated-package checks pass.
The 138-entry docset covers all 101 exports, guides and eighteen examples, with
zero checker problems and actual export/SETF lookup, isolated-install discovery
and guide/example navigation checks on both Lisps. Evidence paths are in the
handoff. Hosted Windows/Linux GUI, real OS session delivery and graphical Cadre
remain unverified. Next define native type/origin contracts without permitting
tag mutation to authorize unsafe native casts; broader ownership remains C1 work.

## Read-only native event type and origin identity (2026-10-28)

ABI 21 adds generic EVENT-ORIGIN-ID and current-context EVENT-NATIVE-TYPE reads.
Origin is an opaque registered bridge identity, not a native pointer or public
receiver. Null/untracked origins map to NIL. Native lookup compares identities
without dereferencing the origin, captures at each callback and grants no
ownership, liveness or historical pointer-lifetime guarantee. Saved snapshots
remain readable after cleanup; context readers preserve nested/expired isolation.
Null/untracked native-origin branches are not exercised by the public synthetic
API, which always supplies a known receiver; that validation gap remains open.
Type/origin writers remain open because unchecked tags can authorize invalid
native casts, and arbitrary origin pointers need retention/lifetime contracts.

The runnable example distinguishes native child origin from parent binding
receiver. Tests cover all supported kinds, standalone/application identity,
nested contexts, cleanup and absence of public writers. Independent review
found no remaining actionable issues. Focused example/tests and all 38 GUI
scenarios plus preflight pass locally on SBCL/CCL. Python and pinned inventory
regressions and five rebuilt relocated-package checks pass. The 141-entry docset
covers all 102 exports, guides and nineteen examples, with zero checker problems,
actual export/SETF lookup, isolated-install discovery and guide/example checks
on both Lisps. Evidence paths are in the handoff. Hosted Windows/Linux GUI,
real OS session delivery and graphical Cadre remain unverified. Next implement
event-category metadata before typed mutation and native ownership prerequisites.

## Native event-category metadata (2026-10-28)

ABI 22 adds EVENT-CATEGORY for snapshots/current contexts and EVENT-CATEGORY-MASK
for compiled :UI/:USER-INPUT/:SOCKET/:TIMER/:THREAD/:ALL values. Mask lookup loads
the native library without starting a GUI session. Category reads are immutable
or innermost-context scoped; no virtual override/setter/selective pump is added.

Verification exposed two gaps between pinned interface docs and implementation:
ALL31 assertion failed because installed3.2.12 includes internal32/64 bits and
returns127. The mapping now queries compiled constants, preserving ALL exactly.
A focused test also corrected the assumption that command/notify inherit UI:
native wxCommandEvent overrides to USER_INPUT, omitted in interface docs. Close
family remains UI. Retain these for C7 reconciliation without exporting internal
category names. Public bits do not imply socket/timer/thread event bindings. The generator
rejected initial per-enumerator status records because it groups this enum body
in one source entry; mappings were corrected to its exact pinned entries.

Independent review found no remaining actionable issue; its stale assertion-message
note was corrected. Focused example/regressions and all 39 GUI scenarios plus
preflight pass locally on SBCL/CCL. Python/inventory regressions and five rebuilt
relocated package checks pass. The 145-entry docset covers all 104 exports, guides
and twenty examples with zero checker problems, actual export/SETF lookup,
isolated installation/discovery and guide/example checks on both Lisps. Evidence
paths are in the handoff. Hosted Windows/Linux GUI, real OS session delivery and
graphical Cadre remain unverified. Next implement bounded registered-tag mutation
using actual native class checks; preserve session fallback isolation and reject
incompatible tags before broader native ownership/origin-retention contracts.

## Bounded registered native tag mutation (2026-10-28)

ABI 23 adds context SETF EVENT-NATIVE-TYPE: registered source+target, same family,
actual wxCommandEvent without wxNotifyEvent for command tags or wxNotifyEvent
for notify tags. Built-in/session/unknown/incompatible tags reject before mutation.
Snapshots report actual current tags; binding identities retain registration tags.
The writer never changes native class/storage, restarts dispatch or guarantees
local callback delivery/order. Native origin and arbitrary tag writers stay open.

An initial focused test incorrectly required an old-tag local observer after
mutation; the backend rematched the changed tag and skipped it. That ordering
assumption was removed. Parent routing verifies changed tags with structural
order for synchronous/queued delivery. Command/notify checks also cover rejected
targets, unchanged state after failure, veto after change and nested/expired
contexts. Public APIs cannot create a mismatched native object with a registered
tag; actual-class rejection is statically reviewed, not directly exercised.

Independent runtime/documentation review found no remaining actionable issue.
All 40 GUI scenarios plus preflight pass locally on SBCL/CCL, as do focused
example/regressions, Python/inventory tests and five relocated-package checks.
The 147-entry docset covers all 104 exports, guides and twenty-one examples, with
zero checker problems, actual export/SETF lookup, isolated discovery/install and
guide/example checks on both Lisps. Evidence paths are in the handoff. Hosted
Windows/Linux GUI, actual OS session delivery and graphical Cadre remain
unverified. Next define origin retention and persistent native event ownership
contracts; preserve actual-class checks and isolated session fallback.

## Decisions

- D1 Keep a bounded first release as an intermediate milestone. The user has
  clarified that the final goal is complete wxWidgets coverage through C1–C7;
  the first release and individual slices do not satisfy final acceptance.
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
