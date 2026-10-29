# Implementation plan

The goal is broad GUI coverage with selective depth: useful desktop workflows,
reliable lifetime/event contracts and Lisp-friendly interfaces. Exhaustive native
wxWidgets parity is no longer the delivery objective. C1–C7 inventories remain
factual discovery records, not a required sequence or release gate.

## Session handoff: broad GUI coverage

The user has superseded the earlier complete-wxWidgets objective. Follow the
[feature policy and acceptance roadmap](docs/WXWIDGETS-COVERAGE.md): core, planned,
on-demand, Lisp-native and deferred. Native pending/partial entries remain visible
but are not automatic blockers. Expand everyday GUI breadth across categories,
implementing actual prerequisites rather than exhausting C1 before other features.

Historical roadmap/checkpoints below preserve their original goal and evidence.
Where they call every pending API required or insist on complete C1 before C2,
that scope is explicitly superseded here. Do not rewrite historical verification
as if later features or platform checks already passed.

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
- Owned-event batch: detached native-backed event construction, independent clone/
  disposal/liveness, mutable command payloads and copy-based queue/process dispatch.
  Final batch verification and independent review pass; exact classes and detached origins only.
- Pending-delivery batch: GUI posting copies, consuming native transfer, local
  synchronous copies and non-application target queue deletion. Final verification
  and independent review pass.
- Standalone-chain batch: canonical next/previous peers, reciprocal guarded SETF,
  unlinked-state query and splice/unlink with unchanged session native ownership.
  Final verification and independent review pass.
- Window-stack batch: managed Get/Push/Pop/Remove, preserving session-owned
  handler lifetime, and explicit stack-aware synchronous copy processing. Final
  verification and independent review pass.
- Command-payload batch: native extra LONG/raw selection-action metadata, uniform
  kind-specific checked/selection SETF and bounded clear-only origin detachment.
  Independent runtime and documentation review pass; final verification is recorded below.
- Handler-client-data batch: EQ Lisp state via native typed token adapters,
  managed foreign-mode protection and session-root cleanup. Independent review
  and final verification pass.
- Binding-user-data batch: per-registration EQ Lisp values, immutable captured
  associations and context-frame lifetime through self-unbind. Independent review
  and final verification pass.
- Idle batch: :IDLE family/resources, bounded RequestMore, session-restored
  automatic mode, own-window opt-in and native wake scheduling. Independent review
  and final verification pass.
- Current bridge ABI: 39. Public API: 202 exports, seventeen window wrapper classes
  (fifteen concrete kinds plus WINDOW/DIALOG abstract bases) plus the event-handler base and frame-owned menu resources, 144 named generics and
  forty-four generic SETF writers. The application-handler is a borrowed view; no
  native application construction or arbitrary-event class binding exists yet; built-ins plus registered command/notify names have
  observer/contextual bindings.
- Latest implementation verification: all 57 GUI scenarios and native preflight
  pass on local Apple Silicon macOS SBCL 2.6.9 and CCL 1.13. Reports:
  `evidence/tree-navigation/{sbcl,ccl}-20261028-final/`. Post-Qt-gate native/focused
  both-Lisp checks pass; wxQt trees are explicitly unavailable, not runtime-verified.
  Five package checks pass in `evidence/packaging/tree-navigation-20261028-final/`.
  Hosted Linux/Windows GUI behavior and the graphical Cadre viewer remain open.

### Feature priorities and working order

1. Preserve core ownership/invalidation, callbacks/main-thread rules, CLOS APIs
   and required documentation/examples/docset/testing for every delivered feature.
2. Preserve the working form/editor slice: Lisp-native open/save, menus/dialogs,
   multiline text, clipboard and notebooks. List boxes and checked tree items
   extend the manager slice. Next add splitter windows for a two-pane tree/list
   manager; retain native OS dialogs as unbound. Add only necessary timer/value/
   lifetime prerequisites; do not drain native ownership/utilities.
3. Expand controls/layout, navigation/notebooks, list/tree, clipboard and drawing
   against form/editor, tree/list manager and drawing/monitoring acceptance.
   Existing examples verify bounded behavior, not those whole applications.
4. Add specialized depth on demand. Prefer Lisp-native utility/model abstractions
   where literal native strings/containers/files/network wrappers add little value.
5. Keep native checkboxes factual and platform claims tied to reports. Retained
   origins, external native adoption and unsupported derived classes stay gaps,
   but need not precede unrelated menus/controls/dialogs. C1–C7 are inventory
   categories, not a delivery sequence. Preserve isolated session rules; never
   synthesize process-exiting live application END_SESSION in ordinary tests.

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
  GUI suites were rerun. Close a selected feature scope only after its agreed
  implementation and verification are complete; category-wide parity is not required.
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

## Phase 6: Maintain the native inventory and GUI feature roadmap

- [x] Inventory the pinned wxWidgets API, including widgets, functions, overloads,
  events, styles, optional components and platform-specific declarations.
- [x] Mark implemented and partial operations in the detailed lists and class
  index, preserving their scopes and Lisp/native mappings across regeneration.
- [x] Group the native inventory into source-linked categories and separate
  feature priorities, verification requirements and scoped acceptance.
- [x] Preserve release preparation as Phase 7 and link the new coverage plan.

The [wxWidgets coverage plan](docs/WXWIDGETS-COVERAGE.md) records the implemented
baseline and inventory categories C1–C7. Its class index and per-header declaration
checklists cover all 398 wxWidgets 3.2.12 interface headers, with source links
and a hash manifest. A persistent status ledger retains checked/partial entries;
compiler scaffolding is context rather than binding tasks. These are
source-inventory entries, not a count of distinct
functions or a claim of full binding coverage. Final source/documentation and
inherited-API reconciliation is an explicit remaining coverage task.

**Acceptance:** Every pinned interface header has categorized, source-linked
entries; existing support is distinguished from complete class/API coverage.
The inventory-plan deliverable is complete. C1/C2 implementation is underway;
remaining pending/partial scope stays visible for GUI feature selection. The
original exhaustive-parity acceptance objective is superseded.

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

## Historical implementation checkpoints (earlier goal superseded)

These dated checkpoints retain original scope/evidence. Complete-parity or strict
C1-before-C2 instructions below describe the former goal; the current handoff and
feature policy supersede them without changing recorded implementation facts.

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

## Larger owned-event construction, cloning and dispatch batch (2026-10-28)

ABI 24 adds OWNED-EVENT, MAKE-EVENT, CLONE-EVENT, DESTROY-EVENT and EVENT-ALIVE-P.
Native-backed CLOS initialization supports subclass slots/defaults/AFTER hooks
and mixed-resource rollback. Clone construction preserves source state over
inherited payload defaults; factory calls reject explicit payload overrides.
Direct MAKE-INSTANCE :CLONE-FROM uses source state over all payload initargs.
Owned objects remain session-registered until explicit destruction or cleanup;
there is no GC finalizer. An independent clone, queue copy or processing copy
survives disposal of its source. Snapshots remain immutable; contexts remain
innermost-callback scoped. EVENT-KIND/FAMILY/TEXT/INTEGER are generic and native
command text/integer SETF now accepts owned values and current contexts.

Only exact wxCommandEvent/wxNotifyEvent/wxCloseEvent classes clone. Borrowed
userdata/client pointers reject, native origin is detached, and effective string
payload is captured before origin removal. Copy dispatch attaches its receiver
as native origin; no source ownership is transferred. Native session event copies
still require isolated standalone receivers. Arbitrary origin writes/retention,
native derived classes, generic wxObject wrappers and direct transfer remain gaps.

Implementation verification caught a renamed BOA snapshot field mismatch, an
overstrict clone-payload check that rejected inherited default KIND, and a rollback
test that needed to pump asynchronous wxFrame destruction. Independent review
found mixed OWNED-EVENT/EVENT-HANDLER ancestry could overwrite handles; both
construction paths now reject it before allocation. Review also found lazy
wxEVT_TEXT GetString can read a text-entry origin instead of stored payload:
context text SETF rejects tracked text-entry/unknown origin cases, detached clones
freeze effective text, and owned text-change copies reject text-entry receivers
before emission. Existing keyword synthetic behavior is unchanged.

Docset generation initially caught a missing class syntax field; it was added
before rebuilding.

Final batch inventory: 109 exports, 69 named generics, 20 generic SETF writers,
41 GUI scenarios, 154 docset entries and twenty-two example sources. Independent
runtime and documentation reviews pass after resolving the findings above.
All 41 local GUI scenarios and native preflight pass on SBCL and CCL. The five
rebuilt relocated-package checks pass. Python/inventory regressions and actual
export/SETF lookup checks pass; docset checker reports zero problems, with isolated
installation, discovery and API/guide/example access verified on both Lisps.
Evidence is in `evidence/owned-events/{sbcl,ccl}-20261028/` and
`evidence/packaging/owned-events-20261028/`. Two user-authorized subagents handled
documentation/coverage and expanded tests in parallel; independent review remained
separate. One test-only STRING= arity error was corrected before passing checks.
Hosted Windows/Linux GUI, real OS-session
delivery and graphical Cadre remain unverified, as do safe-public-API-inaccessible
null/untracked origins and incompatible actual-class rejection branches.
Next define explicit origin retention/ownership or another prerequisite C1 slice
before widening pointer-bearing native events; retain no unchecked native pointer.

## Pending posting, consuming transfer, local copies and cancellation (2026-10-28)

ABI 25 adds POST-EVENT (GUI wxPostEvent/AddPendingEvent copy), TRANSFER-EVENT
(genuine wxQueueEvent handoff), PROCESS-EVENT-LOCALLY (independent local copy)
and DELETE-PENDING-EVENTS (one non-application receiver, no callbacks). Post and
local processing preserve the source; transfer validates before consuming it,
then removes bridge ownership before native handoff. No rollback or delivery
guarantee exists after handoff. Session-isolated receivers, exact supported
classes and lazy wxEVT_TEXT/text-entry receiver restrictions remain in force.

Local processing avoids normal outer global-filter/parent/application fallback,
retaining native local pre-processing/chain behavior. Target deletion rejects the
borrowed application handler and callback/pump/RUN use, removes pending events
without bindings/callbacks, then synchronizes wxApp's pending-handler membership.
Native DeletePendingEvents alone leaves that list entry, so the wrapper uses
RemovePendingEventHandler from the implementation header. It is not present in
the pinned documentation interface and must remain a C7 reconciliation dependency,
not an asserted public binding. wxAppConsole::DeletePendingEvents and per-handler
ProcessPendingEvents remain unbound; the latter requires a nonempty target queue
without a safe public per-handler pending query. Existing app-wide processing
is unchanged.

Final batch counts: 113 exports, 73 named generics, 20 generic SETF writers,
42 GUI scenarios, 160 docset entries and twenty-three examples. Independent
runtime and documentation review pass. Local native static/default handlers can
still execute under local processing, as clarified in the guide/reference.
All 42 GUI scenarios plus native preflight pass locally on SBCL and CCL. Five
rebuilt relocated-package checks pass. Python/inventory regressions, actual
export/SETF lookup, isolated docset installation/discovery and API/guide/example
navigation pass; the docset checker reports zero problems. Evidence is in
`evidence/pending-events/{sbcl,ccl}-20261028/` and
`evidence/packaging/pending-events-20261028/`; package:
`dist/wxlambda-form-pending-events`.

An initial script reused the package text buffer while appending API methods,
temporarily replacing lisp/api.lisp and causing CLASS-NOT-FOUND on example load.
The committed source was restored and only the intended 28 new lines reapplied;
the example and all subsequent checks pass. Expanded regression tests passed on
both Lisps on their first run. Two user-authorized subagents handled docs/coverage
and regressions in parallel, with independent review separate.
Hosted Windows/Linux GUI, actual OS-session delivery, graphical
Cadre and prior inaccessible pointer/class-mismatch branches remain unverified.
Next continue C1 origin retention/ownership or handler-chain prerequisites;
worker posting, arbitrary native pointers and global queue cancellation remain gaps.

## Managed standalone handler chains (2026-10-28)

ABI 26 adds NEXT-HANDLER/PREVIOUS-HANDLER generic readers and SETF, plus
HANDLER-UNLINKED-P and UNLINK-EVENT-HANDLER. Sources/peers are owned standalone
handlers only; borrowed window/app inputs reject before mutation. Reciprocal
vacant endpoints can join whole segments, existing links are idempotent, and
self/cycle/occupied-endpoint attempts reject unchanged. SETF NIL splits one edge;
UNLINK splices neighbors and leaves the node alive. Native ownership remains
session-owned. A session map strongly roots canonical wrappers until explicit
destruction, rollback or cleanup so peer lookups return EQ original subclasses.

Reads are callback-safe under lifetime guards. All structural writes reject
RUN/pump/callback use. Linked callback-time destruction rejects; outside callbacks
native destruction unlinks/splices normally, while unlinked callback disposal
retains existing deferral. Normal/local/pending dispatch traverses native chains;
disabled nodes do not disable followers. Chain-node traversal follows next links;
callback order within one node is still native behavior. Queues belong to target
nodes, not all future chain observers. Native session tags remain isolated from
app fallback. Constructor rollback cleans created resources/canonical roots, not
arbitrary edits to external graph state: native Unlink can leave surviving
prefix/suffix segments joined after the failed node is removed.

The pinned setter prose forbids NULL, while implementation setters and native
Unlink permit paired pointer clearing. NIL is a bounded reciprocal detach mapping,
not a one-sided raw setter; retain the discrepancy for C7 reconciliation. Native
Push/Pop/Remove, borrowed chains, external adoption and arbitrary origin retention
remain open.

Final counts: 117 exports, 77 named generics, 22 generic SETF writers,
43 GUI scenarios, 166 docset entries and twenty-four examples. Independent runtime
and documentation review pass with no actionable findings. The example and
expanded focused regressions pass on SBCL and CCL without implementation/test
corrections. All 43 local GUI scenarios plus native preflight and five rebuilt
relocated-package checks pass. Python/inventory regressions and actual export/SETF
lookup pass; docset checker reports zero problems, including isolated install,
discovery and API/guide/example navigation on both Lisps. Evidence:
`evidence/handler-chains/{sbcl,ccl}-20261028/` and
`evidence/packaging/handler-chains-20261028/`; package:
`dist/wxlambda-form-handler-chains`. Two user-authorized subagents handled
regressions and docs/coverage in parallel, with separate independent review.
Hosted Windows/Linux GUI, actual OS-session delivery, graphical Cadre and
previous inaccessible pointer/class-mismatch branches remain unverified.
Next define borrowed window handler-stack ownership before widening chain peers
or adding Push/Pop; preserve reciprocal identity, session lifetime and dispatch
mutation restrictions.

## Managed window handler stacks and explicit routing (2026-10-28)

ABI 27 adds WINDOW-EVENT-HANDLER, PUSH/POP/REMOVE-EVENT-HANDLER and copied
PROCESS-WINDOW-EVENT/LOCALLY. Push requires unlinked unattached owned standalone
handlers and preserves bridge native ownership/canonical roots. Default pop and
remove detach alive; deleting pop releases registry ownership before native
Pop(true) deletion and clears roots/bindings. Empty pop rejects; valid detached
or other-window remove probes return NIL without native assertion. Getter returns
canonical pushed handler or the identical window when empty, never an adopted
external native pointer.

All structural stack operations reject RUN/pump/callback use. Attached nodes
reject general next/previous reads/SETF and Unlink, avoiding exposure of borrowed
window tails; IsUnlinked returns false. Attached handler destruction removes it
before outside-callback deletion; linked/attached callback deletion rejects.
All eight concrete native widget types use internal managed destructors that
Pop(false) before wxWindowBase teardown, so handlers survive actual window or
ancestor destruction independent of destroy-event Skip/enabled flags. A destroy
request is not completed detachment; asynchronous native deletion still applies.
Session finish detaches stacks before window cleanup. Native window teardown
requires stack removal and does not take deleting ownership of these handlers.
Constructor rollback cleans resources, not arbitrary restoration of external edits.

Explicit window processing enters managed pushed stacks; generic/local/queued
window APIs retain wxEvtHandler stack bypass. Copy origin is the window and
snapshot receiver can be a pushed handler. Session binding/delivery on attached
handlers rejects; push also rejects active session bindings or queued session
events, tracked until native processing/deletion. No real OS session event,
worker facility, arbitrary SetEventHandler or native virtual Lisp hook is added.

An initial C++ variable-shadow build error was corrected before example checks.
Coverage validation rejected approximate ProcessWindowEvent source lines; exact
pinned declarations at lines 2669/2682 were used before regeneration. Broad focused
regression/example passes both Lisps (quiet-run.kZR9FBII), covering all eight
widget destructors/parent cascade with disabled event processing, session guards,
pending delivery after detachment and rollback directions. Public wxEVT_DESTROY
binding is absent: consuming a real destroy notification remains untested, while
ManagedWindow destructor cleanup is structurally independent of observer Skip.

Final counts: 123 exports, 83 named generics, 22 generic SETF writers,
44 GUI scenarios, 174 docset entries and twenty-five examples. Independent runtime
and documentation reviews pass with no remaining actionable findings. All 44
local GUI scenarios plus native preflight pass on SBCL and CCL. The five rebuilt
relocated-package checks pass. Python/inventory regressions, actual export/SETF
lookup, isolated docset install/discovery and API/guide/example reading pass;
checker reports zero problems. Evidence:
`evidence/window-handler-stacks/{sbcl,ccl}-20261028/` and
`evidence/packaging/window-handler-stacks-20261028/`; package:
`dist/wxlambda-form-window-handler-stacks`. Two user-authorized subagents handled
tests and docs/coverage in parallel; independent review remained separate.
Hosted Windows/Linux GUI, native OS-session delivery, graphical Cadre and
prior inaccessible pointer/class-mismatch branches remain unverified. Continue
C1 external stack adoption/retained origin or other lifetime prerequisites before
exposing arbitrary borrowed native chains.

## Command payload views and bounded origin clearing (2026-10-28)

ABI 28 adds EVENT-EXTRA-LONG and raw EVENT-SELECTION-P saved/current/owned readers;
extra LONG SETF uses signed native LONG bounds, is independent of signed32 integer
and drives raw IsSelection (nonzero extra flag). It is not a kind/index-validity
predicate: a native choice event can have extra0 and flagfalse. Owned construction
accepts command/notify-only :EXTRA-LONG; keyword synthetic dispatch does not. Clone
source state wins defaults and factories reject explicit clone extra overrides.

EVENT-CHECKED-P/EVENT-SELECTION become uniform kind-specific generic views while
preserving snapshot meanings. Live checked SETF is :TOGGLE-only, generalized bool
stored as 0/1; selection SETF is :SELECTION-only, NIL as -1 or nonnegative signed32
index. Raw integer writes can expose other negatives; only -1 maps to NIL. Direct
setters change event fields, not control setters/item lists; later handlers can
react, so whole-dispatch control state is not guaranteed unchanged.

Clear-only EVENT-ORIGIN-ID SETF accepts NIL. Owned/actual-null origins are idempotent;
non-null current origins require registered compatible command/notify native class.
Built-in/nonregistered non-null origins reject, even if the bounded reader returns
NIL for untracked state. No object is dereferenced, retained or destroyed; source
receiver and old snapshots stay unchanged, later snapshots can capture NIL.
Arbitrary new origins, retained objects and user/client-data ownership remain gaps.

Verified counts: 125 exports, 87 named generics, 26 generic SETF writers,
45 GUI scenarios, 178 docset entries and twenty-six examples. Independent runtime,
documentation and final test-adjustment reviews found no actionable issues. Both
local Lisp GUI suites and native preflights pass, as do five relocated package
checks, sixteen Python tests, twelve coverage-inventory tests, and the focused
example/regressions on both Lisps. Docset checking reports zero problems; actual
public exports/SETF coverage, isolated installation, discovery, symbol lookup and
guide/example access pass. Reports: evidence/command-event-payloads/{sbcl,ccl}-20261028-final/
and evidence/packaging/command-event-payloads-20261028/; package:
dist/wxlambda-form-command-event-payloads.

The initial full-suite run caught an obsolete origin test asserting no writer
exists. Updated it to require the new live writer and verify saved snapshots
remain immutable; independent review passed and both full suites were rerun.
Hosted Windows/Linux GUI, actual OS
session delivery, graphical Cadre and prior inaccessible pointer/class-mismatch
branches remain unverified. Continue C1 retained origin/client/userData lifetime
or broader native class prerequisites; do not equate raw flags with full derived
payload semantics.

## Managed handler Lisp client-data roots (2026-10-28)

ABI 29 adds EVENT-HANDLER-CLIENT-DATA/SETF for standalone handlers, windows and
the managed application. A native wxClientData subclass owns only a numeric root
token; the Lisp session table retains the actual object by EQ. NIL clears,
replacement/clear releases prior roots, and failed candidate assignments remove
their root. No constructor initarg, Lisp/native pointer exposure, event payload,
per-item client data, generic wxClientData wrapper or untyped binding is added.

Foreign non-null typed objects and all untyped/Void storage modes (even NULL raw
pointer) reject untouched. Fresh None/typedNULL read NIL. Private native interfaces
on managed App/StandaloneHandler/ManagedWindow expose protected m_clientDataType
before GetClientObject because wxEvtHandler has no public mode getter. Retain that
implementation-header dependency for C7; do not export the protected field.
Native asynchronous destruction can retain Lisp roots until actual deletion and
next client/generic cleanup, rollback or session end; the session table clears
after finish. Detached pushed handlers keep their own datum; deleting handlers
or windows releases their separate association. Returned ordinary Lisp references
remain valid. Immutable snapshots do not capture handler client data.

Verified counts: 126 exports, 88 named generics, 27 generic SETF writers,
46 GUI scenarios, 181 docset entries and twenty-seven examples. Independent
runtime/harness and documentation reviews pass. Both local full GUI suites and
native preflights, the example/focused regressions on SBCL and CCL, five relocated
package checks, sixteen Python tests and twelve coverage-inventory tests pass.
The native foreign typed/untyped/voidNULL regression passes with zero captured
wx assertions (quiet-run.xAmVX0T2). Build the optional WXLAMBDA_BUILD_NATIVE_TESTS
target wxlambda-client-data-native from tests/client-data-native.cpp; commands
are in docs/HANDLER-CLIENT-DATA.md. The local separate build used
/private/tmp/wxlambda-client-data-native-build and the pinned
/opt/homebrew/opt/wxwidgets@3.2/bin/wx-config-3.2.

Docset checking reports zero problems, with actual public-export/SETF coverage,
isolated installation, discovery, symbol lookup and guide/example access verified.
Reports: evidence/handler-client-data/{sbcl,ccl}-20261028/ and
evidence/packaging/handler-client-data-20261028-final/; final package:
dist/wxlambda-form-handler-client-data-final. An early Python run preceded the API
reference update and failed its export-reference consistency checks; the final
run passed after documentation generation. No runtime test failures occurred.
Hosted Windows/Linux GUI, actual OS
session delivery, graphical Cadre and prior inaccessible pointer/class-mismatch
branches remain unverified. Continue C1 event/userData, retained-origin or broader
client-data ownership contracts explicitly; this handler slot does not complete
generic client containers or derived control item data.

## Per-binding Lisp user-data associations (2026-10-28)

ABI 30 adds BIND-EVENT :USER-DATA and read-only EVENT-USER-DATA for snapshots,
current contexts and safe owned-event NIL reads. Non-NIL values are retained by
EQ in callback association roots; native entry-owned wxObject adapters contain
only numeric binding tokens. Native scope captures the token before callback and
Lisp dispatch frames retain the value, so self/peer unbind and GC do not invalidate
active context data. Nested contexts suspend/restore outer lifetime as before.
Snapshots preserve the reference after unbind/session end, not a deep immutable
copy of mutable contents. No callback arity change or SETF association API exists.

Handler client data and native command/client/item payloads remain separate.
Owned events cannot adopt borrowed binding pointers and current borrowed clones
still reject even after self-unbind without dereferencing the freed adapter.
NIL-data bindings keep prior clone behavior. Rebind to change associations.
For generalized GETF places, bind EVENT-USER-DATA result locally before mutation;
initial example direct INCF(GETF(reader)) required nonexistent SETF and was fixed.
An initial native harness compile used nonexistent SetEventUserData; test-only
internal injection replaced it without widening public APIs. Runtime native/Lisp
behavior had no issue from those test/example corrections. Extended optional
wxlambda-client-data-native regression checks captured context/self-unbind,
expiration and owned borrowed-pointer guards; require zero native assertions.

Verified counts: 127 exports, 89 named generics, 27 generic SETF writers,
47 GUI scenarios, 184 docset entries and twenty-eight examples. Independent
runtime/harness review passes. Documentation review caught stale no-userData
scope wording; the old Bind/Unbind and accumulated class scopes were rewritten,
regenerated and independently re-reviewed with no remaining findings.

Both local full GUI suites and native preflights pass, including the final
application/custom-command, registered-notify and isolated close/session-family
matrix with Lisp zero and list data. No application session events were synthesized.
Focused regressions/example pass on both Lisps, as do five relocated package
checks, sixteen Python tests and twelve coverage-inventory tests. The extended
optional native regression passes with zero wx assertions (quiet-run.dGuk7Wpl),
covering captured context tokens after self-unbind, expiry and borrowed-pointer
owned/clone rejection. Docset checking reports zero problems; actual public-export
and SETF coverage, isolated installation, discovery, symbol lookup and guide/example
access pass. Reports: evidence/binding-user-data/{sbcl,ccl}-20261028-final/ and
evidence/packaging/binding-user-data-20261028/; package:
dist/wxlambda-form-binding-user-data.
Hosted Windows/Linux GUI, native OS-session
delivery, graphical Cadre and prior inaccessible pointer/class-mismatch branches
remain unverified. Continue C1 retained-origin/native client-item ownership
or broader lifetime prerequisites without exposing arbitrary native pointers.

## Broad GUI selective-depth scope and bounded idle (2026-10-28)

The user explicitly replaced exhaustive wxWidgets parity with broad GUI coverage
and selective depth. Current AGENTS/README/roadmap/generator text now separates
native factual status from core/planned/on-demand/Lisp-native/deferred feature
priorities. C1–C7 are inventory categories, not delivery order; pending native APIs
are not automatic release blockers. Dated ledgers preserve earlier goal language
as superseded history. Representative form/editor, tree/list manager and drawing/
monitoring applications define acceptance; selected platform claims require reports.
The immediate post-idle vertical slice is menus/actions plus standard dialogs,
not additional ownership/utilities unless that GUI feature actually needs them.

ABI 31 adds native :IDLE/family :IDLE snapshots/current/owned resources, idle-only
RequestMore SETF, zero-argument global IDLE-MODE/SETF restored at finish, own-window
wxWS_EX_PROCESS_IDLE property preserving other bits, and GUI-thread WAKE-UP-IDLE
without inline pumping. Native mode/visibility/app rules control automatic idle;
explicit synthetic delivery bypasses that policy. ProcessPendingEvents does not
generate idle, while ProcessEvents can Yield/ProcessIdle. Bound work and stop
RequestMore when finished; no timer/busy-loop/count guarantee. CanSend is absent
from the pinned interface and is not mapped. Owned MORE-REQUESTED-P initargs are
idle-only, source state wins clone defaults and keyword synthetic dispatch adds
no new payload option.

The inventory regression initially retained an assertion for the superseded
full-parity requirement; it was replaced with feature-priority/status-separation
checks before final validation.

Verified counts: 131 exports, 92 named generics, 30 generic SETF writers,
48 GUI scenarios, 190 docset entries and twenty-nine examples. Independent
runtime/native-harness and roadmap/idle-documentation reviews pass. Documentation
review found an omitted wxIdleEvent constructor attribution on OWNED-EVENT; fixed
and rechecked. No runtime regression failures occurred.

Both local full GUI suites and native preflights pass, including the final idle
window-stack/user-data integration tests. Focused regressions/example pass on
SBCL and CCL; native regression passes with zero wx assertions
(quiet-run.e4oXnAfe). Mode restoration on ordinary finish is runtime-tested;
exception-path restoration is statically reviewed rather than fault-injected.
Thirteen inventory tests and sixteen Python tests pass. Five relocated package
checks pass. Docset checking reports zero problems, with actual public-export
and SETF coverage, isolated installation, discovery, symbol lookup and guide/example
access verified. Reports: evidence/idle-events/{sbcl,ccl}-20261028-final/ and
evidence/packaging/idle-events-20261028/; package: dist/wxlambda-form-idle-events.

Hosted Windows/Linux GUI, real OS-session delivery, graphical Cadre and previous
inaccessible pointer/class-mismatch branches remain unverified. Native omissions
remain factual inventory entries, not automatic blockers for unrelated GUI work.
**Next batch: menus/actions plus standard dialogs for the form/editor workflow,**
with only the value/lifetime prerequisites that feature needs. The earlier saved
native-only idle checkpoint is now integrated; do not reapply its patch.

## Frame menus and actions (2026-10-28, verified)

ABI 32 adds the menu portion of the selected everyday-GUI vertical slice.
MENU-RESOURCE is abstract; native frame-owned MENU-BAR/MENU/MENU-ITEM resources
use native-backed CLOS and generic factories with :PARENT, subclass hooks and
rollback. Menu/tab labels, normal/check/separator items, per-bar unique positive
IDs, counts/kind/identity, scalar label/enabled/check updates and explicit
destruction are bounded managed interfaces. The native owner tree invalidates
descendants independently of Lisp GC. Structural edits reject callbacks/pumps/RUN;
scalar edits remain available during callbacks. No adoption, reparenting, popup,
submenu/radio group or generic native ownership interface is added.

Built-in :MENU command callbacks use the existing frame ID/range/user-data/context
protocol. EVENT-CHECKED-P reads/writes :MENU/:TOGGLE raw command metadata without
looking up or synchronizing resource state. ACTIVATE-MENU-ITEM uses native frame
ProcessCommand; check resources toggle before callbacks, disabled items return
NIL, separators reject. Normal native commands can carry integer -1, so raw
EVENT-CHECKED-P can be true on a normal item; resource CHECKED-P is check-only.
OS menu selection
and accelerators remain separate verification. Normal/check label/help validation
and separator option restrictions are documented in docs/MENUS.md.

Independent native review identified a P2 ABI-boundary issue: menu-label string
reads needed the common metadata_string catch-all and stale-error clearing,
rather than the narrower manual exception path. The implementation now uses the
shared boundary; the native regression passes with zero wx assertions
(quiet-run.A3Ze5r3X). Documentation review corrected built-in type enumeration,
managed-menu origin identities, stale menu liveness scope and obsolete app idle/
propagation gaps. Initial docset construction caught missing MENU-RESOURCE syntax;
that signature was completed. The actual-package export/class check then caught
the abstract MENU-RESOURCE entry missing its required Construction section
(quiet-run.8UQaBuEp); an explicit abstract construction/error contract was added,
and all four menu class records now include Construction sections. Standard
static docset checks alone did not catch this actual-class requirement.
An initial test accumulator DEFVAR missing NIL was
corrected in the tests; no runtime failure was attributed to that test setup.

Verified counts: 146 exports, 103 named generics, 31 SETF generics,
49 GUI scenarios, 30 examples and 207 docset entries. Independent runtime/native
and documentation reviews pass after the corrections above. Both local full GUI
suites and native preflights pass on SBCL and CCL, including menus and the final
label boundary fix. Focused tests and the example pass on both Lisps; the native
regression reports zero wx assertions (quiet-run.A3Ze5r3X). Thirteen inventory
tests, sixteen Python tests and five relocated package checks pass. Actual-package
export/class/SETF checks and isolated docset installation, discovery, symbol lookup
and guide/example access pass; the docset reports zero problems.

Evidence: evidence/menus/{sbcl,ccl}-20261028-final/ and
evidence/packaging/menus-20261028/; package: dist/wxlambda-form-menus.
Manual OS menu selection/accelerator input, hosted Windows/Linux GUI, real OS
session delivery and graphical Cadre remain unverified. Standard dialogs are the
next selected GUI slice; menu radio/submenu/popup depth remains on-demand rather
than an automatic blocker for that next feature.

## Modal text-entry and single-choice dialogs (2026-10-28, verified)

ABI 33 adds abstract DIALOG(window), native-backed TEXT-ENTRY-DIALOG and
SINGLE-CHOICE-DIALOG, keyword-only factories, modal show/end/state and generic
title/text value/choice selection accessors. Parent is optional; strings are
Unicode without NUL, choice items nonempty with valid index(default0). Native
styles/position are fixed. Direct subclass construction, defaults/AFTER hooks,
initialization rollback and managed window invalidation follow existing contracts.
Construction sections are required for all three class docset records.

SHOW-MODAL permits entry from menus/RUN/pumps/callbacks but rejects nested modal
loops and already shown dialogs. Outer contexts suspend during native modal
processing; inner callbacks retain scoped lifetime. Unhandled callback errors
cancel then re-signal after unwind; locally caught nested notifier errors can
recover. Active dialog/ancestor destruction rejects until SHOW-MODAL returns,
even after END-MODAL makes native IsModal false. Explicit manual pumping/lifecycle
and structural menu edits remain guarded. These dialogs are modal-only; generic
SHOW-WINDOW/SHOWN-P writers reject. END-MODAL is raw result assignment, not button
validation/transfer. Native GetValue/GetStringSelection stored/cache timing must
not be mistaken for live edit/index synchronization. See STANDARD-DIALOGS.md.

A macOS exploratory wxMessageDialog/FileDialog cancellation probe hung past the
five-second watchdog even with IsModal false; these families are deferred until
a safe native cancellation bridge exists. Text/choice cancellation completed
promptly in the bounded probe. This is evidence about that local probe only.
No message/file public API, graphical acceptance or cross-platform claim is made.

The final native modal close guard consumes current-dialog close and cancels it;
ancestor close is consumed, while public CLOSE-WINDOW on a protected ancestor
rejects. This supplements direct destruction guards against queued/native close.
An initial example used a two-argument observer without :WITH-CONTEXT; the primary
arity error correctly cancelled/unwound the modal loop without a hang, and the
example callback was corrected to the one-argument observer contract.

Verified counts: 158 exports,110 named generics,34 SETF generics,
50 GUI scenarios,31 examples and221 docset entries. Independent runtime/native
harness and documentation reviews pass. Documentation review clarified explicit
parent normalization (hidden requested parent retained; NIL truly parentless)
and outer-context suspension throughout the modal boundary. Native data-transfer
semantics were verified with actual OK-button handler dispatch separately from
raw END-MODAL. The initial test wrong-parent fixture used an incorrect MAKE-EVENT
call and was corrected so it exercises the intended receiver rejection.

Pre-repair checks, retained as historical evidence: focused regressions and the
bounded example pass on SBCL and CCL (quiet-run.0emq63UB / quiet-run.JjAYhbfH). Both full 50-scenario GUI suites and
native preflights pass (quiet-run.AOqbKVam); the watchdog-bounded native harness
passes with zero wx assertions (quiet-run.16uKkU8j). Thirteen inventory tests,
sixteen Python tests and five relocated package checks pass. Actual-package
export/class/SETF checks and isolated docset installation, discovery, symbol lookup
and guide/example access pass; docset checking reports zero problems.
Evidence: evidence/standard-dialogs/{sbcl,ccl}-20261028/ and
evidence/packaging/standard-dialogs-20261028/; package:
dist/wxlambda-form-standard-dialogs.

The interactive START prompt/physical input, hosted Windows/Linux GUI, real OS
session delivery and graphical Cadre remain unverified; EXERCISE and native modal
accept/cancel paths were automated. Native message/file panels are not delivered:
complete a tested cancellation approach (platform bridge or documented generic
implementation) next, preserving callback/lifetime guarantees. Continue selected
GUI breadth without exhausting unrelated event/native utility declarations.

### Late scheduled-deletion review finding (repaired and verified)

After the first green 50-scenario runs, a direct native probe found that an
ordinary wxFrame::Destroy request can enter wxApp's scheduled-deletion list while
IsBeingDeleted still returns false. A child SHOW-MODAL incorrectly entered its
loop under that queued-for-deletion ancestor and hit the five-second watchdog
(quiet-run.g92SgChO). Prior green reports did not cover this branch and do not
establish the repair.

The repair combines bridge pending markers with the public
wxApp::IsScheduledForDestruction query and validates the complete native ancestor
chain for window/resource availability and modal preflight. Handler/menu liveness
also follows the owning native hierarchy. A regression queues parent deletion
before SHOW-MODAL and requires rejection before modal entry. This helper is a
public wxWidgets interface dependency, not a private C7 implementation-header
dependency and not a new exported general deletion-list query. Native rebuild and zero-assert harness now pass (quiet-run.isHjwh64 and
quiet-run.ybnY3Eko). The harness proves both public Destroy and direct native
ScheduleForDestruction on a parent reject child SHOW-MODAL before loop entry,
leave output unchanged/no active modal and make parent/child/menu liveness false.
The bridge deferred-destroy marker survives through native Destroy via RAII cleanup.
Independent review clears the repair. Final expanded focused regressions pass on
SBCL and CCL (quiet-run.RA2WbLD1), including immediate and callback-deferred parent
Destroy before modal entry. Both full 50-scenario suites and native preflights
pass after the repair (quiet-run.QKI5KW29). Thirteen inventory tests, sixteen
Python tests, five relocated package checks and actual-export/class/SETF checks
pass. The 221-entry docset reports zero problems; dedicated installation,
discovery, symbol lookup and guide/example access pass. Final reports:
evidence/standard-dialogs/{sbcl,ccl}-20261028-final/ and
evidence/packaging/standard-dialogs-20261028-final/; final package:
dist/wxlambda-form-standard-dialogs-final. Earlier pre-repair artifacts/reports
are retained for diagnosis, not as proof of the corrected lifetime behavior.

## Generic message/file workflows (2026-10-28, verified)

ABI34 adds MESSAGE-DIALOG and FILE-DIALOG via explicit wxGenericMessageDialog/
wxGenericFileDialog backend, with no :BACKEND selector. Native OS NSAlert/NSPanel
remain unbound; prior watchdog probe evidence is retained, not erased by generic
support. Both native-backed DIALOG/WINDOW families preserve direct CLOS/defaults/
AFTER/rollback, hidden/parentless requested-parent normalization and scheduled
receiver/ancestor deletion guards. Both new class entries have Construction
sections. Generic message body/buttons and file mode are immutable; path/filter
index use generic SETF. File path/directory pathnames convert via native namestring;
path getter is current native state, not accepted-result or content-IO proof.

Message results extend SHOW/END-MODAL with offered YES/NO; CANCEL is always a safe
unwind. File selection is single-only with validated description/pattern filters,
mode-appropriate must-exist/overwrite flags and directory preflight. Raw END-MODAL
still bypasses UI validation/transfer and existence/overwrite enforcement. Browser
New Directory may create directories immediately: no zero-filesystem-effect claim.

Internal native prompts are redirected to generic message dialogs through a
workflow modal hook. Depth scopes suspend borrowed contexts; errors/public
cancellation unwind inner prompts before outer dialogs sequentially. A native
probe ending all nested loops at once hung; the supported bridge instead waits
for each inner unwind before ending the outer loop. Inner user
Cancel returns to the outer workflow normally. Handled notifying errors at the
same depth may recover; errors across a new modal boundary cancel the workflow.
Cancellation requested before native loop entry is remembered. Public nested
SHOW-MODAL remains forbidden. Native property calls that open internal prompts
use the same safety boundary.

Implementation dependencies: generic implementation headers/protected eager
message control creation (for AFTER-hook native properties), native modal-hook
overrides, generic HasFdFlag and restoring file SAVE/OVERWRITE bits that overlap
Cocoa wxDialog button styles. Public inventory equivalents remain partial; generic
classes absent from the pinned documentation interface are not invented as full
public declarations. See MESSAGE-FILE-DIALOGS.md for supported scope.

Independent documentation review corrected inherited dialog class scopes to
distinguish deferred platform-native backends from supported generic message/file
workflows. The END-MODAL contract explicitly rejects successful outer completion
while an internal prompt is active; only CANCEL requests whole-workflow unwind.
Example checks pass both local Lisps (quiet-run.9i9bojKS). A focused CCL fixture
thread-scope issue is test setup, not a runtime finding; final regressions pass on both Lisps. The private native-depth helper now checks active application
before FFI access, preserving out-of-session notifier errors without unloaded
symbol lookup.

A late private error-handoff review hardened the interval after a synchronous
notifier captures a condition but before Lisp receives it: native code could
otherwise open another prompt while callback dispatch was suppressed. Every CFFI
callback error now sets a private native latch; CHECK-CALLBACK-ERROR clears it
before rethrow. The modal hook skips new native/generic prompts while pending,
and SHOW/constructor helpers drain the primary condition before UI/allocation.
No public exports/counts or general native error-state API are added. Native
rebuild (quiet-run.6HQUrMhC) and zero-assert harness (quiet-run.boeZUNDg) prove
latched native/generic prompt suppression and re-enabled prompting after clear.
Final post-handshake suites pass all 51 scenarios on both SBCL and CCL
(evidence/message-file-dialogs/{sbcl,ccl}-20261028-final/). Focused handoff and
recovery regressions also pass both (quiet-run.gvmBf8Al). Independent runtime
review found no outstanding actionable findings. Five package checks pass
(evidence/packaging/message-file-dialogs-20261028/); package:
dist/wxlambda-form-message-file-dialogs. No native IO-race/allocation-failure
injection coverage is claimed.

Verified counts: 167 exports, 115 named generics, 36 SETF generics, 51 GUI
scenarios, 32 examples and 232 docset entries. Actual-export/class/SETF checks,
zero-problem docset validation, isolated install/discovery/lookup and 16 Python
tests pass. Physical interactive START, hosted Windows/Linux GUI, real OS session
delivery and graphical Cadre remain unverified. Next compose form/editor menu
open/save workflows with Lisp-native document models/content IO; exhaustive
native event depth is not a prerequisite.

## Decisions

- D1 (superseded) The former complete-wxWidgets objective treated bounded releases
  as intermediate and every native gap as required; retained as historical context.
- D11 Current goal is broad GUI coverage with selective depth. Feature policy and
  representative-app acceptance govern delivery; C1–C7 are inventory categories
  and native pending entries are not automatic release blockers.
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

## Persistent document form (2026-10-28, verified)

examples/document-form.lisp composes the existing controls/menus/generic dialogs
into a persistent form. No native binding or public library export changes; ABI34,
167 exports, 115 named generics and 36 SETF generics remain. The new guide and example
bring the docset to 234 entries and 33 examples; full GUI runner now has 52 scenarios.
The UTF-8 four-line format is parsed without the Lisp reader. Complete reads precede
UI installation; writes stage a unique sibling before replacement. Dirty state
compares fields to their loaded/saved baseline. Open/vetoable Close offer Save,
Discard or Cancel; failed/canceled Save blocks replacement/close. Busy guards
reject reentrant commands; forced closing follows native defaults without prompts.
No durability, metadata preservation, external-change detection, undo/autosave or
cross-platform atomic replacement is claimed. This is a form, not a multiline editor.

Independent review found native chooser strings parsed as Lisp pathname syntax,
which could reinterpret literal backslashes/wildcards and select a different file.
Fixed with UIOP:PARSE-NATIVE-NAMESTRING and native basename formatting/preselection;
literal native filename tests cover the original risk. Re-review has no outstanding
actionable findings. Focused both-Lisp checks pass (quiet-run.GDQdEPGc), including
real idle-canceled save, unsaved-close and error dialogs, resumed outer close veto,
scripted save-before-open, failed IO/state preservation and temporary cleanup.
One initial menu test exposed observer Skip semantics; example menu bindings now
consume their routed commands contextually. A CCL test-only nonstandard character
name was replaced by CODE-CHAR. Runner unit result count was corrected for the new
scenario plus native preflight. Final review also aligned reader CR/LF rejection
with writer validation; the malformed-name regression preserves controls/path.
Re-review is clear. All 52 GUI scenarios plus native preflight pass both Lisps
(evidence/document-form/{sbcl,ccl}-20261028-final/, quiet-run.Nt0AVng9). The final
CR/LF fix landed during that suite; a separate post-fix focused run passes both
(quiet-run.ex5y1flw), including example EXERCISE and actual dialog adapter loops.
Five package checks pass (evidence/packaging/document-form-20261028/,
quiet-run.icnlYsqk); package: dist/wxlambda-form-document-workflow. This packages
the existing form executable, not the new standalone example/docs. Docset build,
zero-problem check, isolated install/discovery/lookup and actual 167-export/class/
SETF coverage pass with 234 entries on both Lisps; 16 Python unit checks pass
(quiet-run.j9tqt7YB). Generated docset rebuilt after final source/note edits.

Native inventory statuses stay unchanged: this Lisp composition does not bind
wxDocument/wxDocManager or native streams. Hosted Windows/Linux, physical interactive
input, graphical Cadre, allocation/IO-race injection and real OS session delivery
remain unverified. Next selected slice: bounded text clipboard support, preserving
Lisp-native document/model semantics rather than draining native document APIs.

## Text clipboard and form Copy/Paste (2026-10-28, verified)

ABI35 adds CLIPBOARD-TEXT (zero-argument generic reader and generic STRING SETF),
CLIPBOARD-TEXT-AVAILABLE-P and FLUSH-CLIPBOARD. Counts: 170 public exports, 116 named
generics, 37 SETF generics, 53 GUI scenarios, 34 examples and 239 docset entries.
The standard clipboard is selected internally, with primary preference restored;
no PRIMARY API, raw Open/Close, Clear, clipboard class or data-object ownership is
exposed. Each successful open closes on success/failure; already-open and reentrant
access reject. Native event-context borrowing is suspended during transfer; callback
conditions rethrow only after cleanup and take precedence over native errors.
Read distinguishes NIL/no supported text from available empty text, copies explicit
UTF-8 lengths and rejects NUL. Writes replace all formats and do not auto-flush;
Flush returns false for unsupported/unsuccessful persistence. No rollback of prior
clipboard side effects or cross-platform exit persistence is claimed.

Native harness substitutes an in-memory clipboard only in the regression executable;
production has no injection export or compiled test override. It covers Unicode,
empty/no-text/large data, legacy fallback, Open/GetData/SetData failures, foreign locks,
PRIMARY restoration, nesting, malformed arguments, worker rejection and flush results.
Ownership paths are separately exercised: pinned MSW/OLE failure leaves caller
ownership; reviewed GTK/macOS ownership differs. Native test passes zero wx assertions
(quiet-run.j3pnPBPK); main bridge build passes (quiet-run.aY6xRyn0).

Lisp focused tests pass both runtimes (quiet-run.vFvOcvJ6): copied data, generic writer
return/type/NUL behavior, error text preserved across rooted-binding cleanup,
callback-condition precedence and form Copy/Paste dirty/multiline/empty handling.
Automatic tests never read or replace system clipboard content: only the live
availability query runs; all transfers use native/Lisp doubles. START is interactive;
EXERCISE is availability-only and runs in the focused scenario. The persistent form
adds Edit Copy/Paste name through the existing busy/error handling, without native
selection/Cut or keyboard editing guarantees.

Independent review is clear after fixing accidental script-buffer API-file truncation
(before Lisp testing), error-text loss across callback cleanup, MSW/OLE failure
ownership, generic SETF protocol and platform-specific mock ownership defaults.
Test fixture parentheses and inventory class expectations were corrected during
verification. Regenerated inventory +13 checks and 239-entry zero-problem docset
pass (quiet-run.f8mcDrYL). All 53 GUI scenarios plus native preflight pass both
Lisps (evidence/clipboard/{sbcl,ccl}-20261028-final/). Five package checks pass
(evidence/packaging/clipboard-20261028/, quiet-run.gUAcIsCn); package:
dist/wxlambda-form-clipboard. This packages the existing form executable; new
standalone example/docs remain repository/docset deliverables. Actual 170-export,
class/SETF reference coverage, 239-entry zero-problem checker, isolated install/
discovery/lookup and 16 Python unit tests pass (quiet-run.Kn00BV6A). Native harness
retains a preexisting typeid/side-effect compiler warning unrelated to this change;
zero runtime wx assertions are required and pass. Docset rebuilt after final notes.

Live system clipboard writes, external-app interoperability/exit persistence,
physical interactive START/menu input, hosted Windows/Linux GUI, graphical Cadre
and allocation/IO-race injection remain unverified. The isolated tests do not imply
those passed. Next selected slice: bounded multiline text-control construction and
basic editing semantics for the editor, not native document/stream parity.

## Multiline text and basic editing (2026-10-28, verified)

ABI36 extends TEXT-CONTROL construction with :MULTILINE (NIL default) and
:EDITABLE-P (T default), preserving native-backed CLOS defaults/subclass AFTER
hooks/rollback and thin :CLASS factories. Six new generic names expose immutable
TEXT-MULTILINE-P, mutable TEXT-EDITABLE-P, TEXT-SELECTION and TEXT-INSERTION-POINT,
TEXT-LAST-POSITION and notifying REPLACE-TEXT. Counts: 176 exports, 122 named
generics, 40 SETF generics, 54 GUI scenarios, 35 examples, 247 docset entries.

The existing text constructor/getter/setter ABI now carries explicit UTF-8 lengths;
constructor/SETF/replacement reject NUL rather than truncating. Whole-value SETF
stays silent by default; :NOTIFY T and native Replace deliver text-change events.
Read-only disables native user typing, not explicit Lisp mutations. Text offsets
are native positions (Unicode units/CRLF bookkeeping), never Lisp string indices.
Selection SETF accepts exactly two ordered nonnegative in-bounds integers; caret
SETF requires an in-bounds integer. Signed-64-bit Lisp bounds prevent CFFI wrapping;
native last-position checks protect narrowing to long. No -1 sentinels, GetRange,
selection clipboard/Cut, undo/redo, rich text or arbitrary styles are exposed.

examples/text-editor.lisp demonstrates multiline text, append-at-native-end,
replace-selection, user-editability toggle and bounded EXERCISE. The existing form
stays single-line; its document file format is not a multiline editor format.
Native raw-argument/UTF8/flags/pointer/range/type/stale-handle tests pass with zero
wx assertions; focused SBCL+CCL CLOS/editor tests also pass (quiet-run.zLjI2XS3).
Additional both-Lisp checks pass empty replacement/deletion/no-op, multiline
notifying SETF, nested borrowed-context suspension/resumption and callback-time
control disposal (quiet-run.fuWLIe4U). Callback errors preserve completed mutations
and allow later recovery. Failed initializer cleanup was tested via saved native
ID: querying an incomplete wrapper's removed handle correctly signals instead.

Independent runtime/source/docs review found no runtime issues; one stale native
class-scope description was corrected. Pinned inventory+13 checks and zero-problem
247-entry docset pass (quiet-run.3zboBIhT). Final regenerated inventory+13 checks,
247-entry zero-problem check, isolated install/discovery/lookup, actual 176-export/
class/SETF coverage on both Lisps and 16 Python unit tests pass (quiet-run.Xq6VYoW3).
All 54 GUI scenarios plus native preflight pass both Lisps:
evidence/text-editing/{sbcl,ccl}-20261028-final/. Five package checks pass:
evidence/packaging/text-editing-20261028/ (quiet-run.Y76XoIUJ); package:
dist/wxlambda-form-text-editing. The package is the existing form executable;
the new multiline example/docs remain repository/docset deliverables. Final
header comments document the stricter 0/1 notification flag and native query fields.
Docset rebuilt after final session-note edits.

Physical typing/selection, IME/grapheme behavior, hosted Windows/Linux GUI, graphical
Cadre, a full multiline document application and live clipboard interop remain
unverified. Next selected slice: bounded notebook navigation for broader everyday
GUI coverage; deeper text/selection/undo APIs remain demand-driven.

## Notebook navigation (2026-10-28, verified)

ABI37 adds native-backed NOTEBOOK and thin MAKE-NOTEBOOK construction, setup-time
ADD-NOTEBOOK-PAGE for fully initialized direct PANEL children, canonical NOTEBOOK-PAGE,
page count and generic SETF page labels. Existing SELECTION specializes on notebooks:
silent ChangeSelection by default, native vetoable SetSelection with :NOTIFY T.
Requested assignment is returned even when vetoed; query actual selection. Counts:
182 exports, 127 named generics, 41 SETF generics, 15 window wrapper classes (13
concrete + WINDOW/DIALOG abstract bases), 55 GUI scenarios, 36 examples, 255 entries.

Native ownership protects attached page panels from direct destruction; notebook/
ancestor teardown invalidates descendants. Page additions require quiescence outside
RUN/pump/callback/modal contexts; no removal/reorder/reparent/icons or arbitrary
styles. Page self-attachment during its initializer rejects to preserve rollback.
Wrappers are rooted before native publication so synchronous AddPage callbacks can
resolve canonical pages; successful adoption survives a contained callback condition.
Native caption mnemonic/length behavior is port-dependent, not exact arbitrary echo.

Two built-in notify event kinds :NOTEBOOK-PAGE-CHANGING/:NOTEBOOK-PAGE-CHANGED preserve
snapshots/context/user-data/propagation contracts. EVENT-SELECTION reads actual
wxBookCtrlEvent metadata (not inherited GetInt); CHANGING semantics are port-dependent,
with pinned Windows old-page caveat. Only CHANGING veto affects navigation. No old
selection accessor, metadata selection writing, owned/synthetic/clone support yet.
There are eleven bindable built-ins but only nine constructible built-ins. Dynamic
registered-kind numbering now begins after both notebook kinds; ABI check protects
mismatched libraries. No native event record field or Lisp snapshot slot was added.

Bridge selection/AddPage and native/propagated notebook callback guards protect
same-book reentry and ancestor destruction/close; synchronous modal/clipboard entry
rejects until return. Native modal hook also suppresses internal prompt entry during
navigation; deleting-origin page callbacks are suppressed. Independent review caught
the initial setter-only guard gap: actual user page events now get origin-based
guards in generic binding dispatch. Native regression dispatches actual derived
wxBookCtrlEvent outside a setter and through frame propagation, proving those checks.

Native harness and both focused Lisp suites pass (quiet-run.9BgHkFBL) with zero wx
assertions. It checks page identity/publication, native events/veto, invalid indices/
ownership/labels, modal/clipboard/reentry guards, synthetic/clone rejection, callback
error recovery and constructor rollback. Test-body parentheses and Boolean Unbind
expectations were corrected; inventory owner parsing was narrowed to avoid assigning
wxBookCtrlEvent methods to wxBookCtrlBase. Independent final docs review corrected
eleven-built-in wording and qualified unchanged-notification behavior as CHOICE-only.
Regenerated inventory+13 checks and 255-entry zero-problem docset pass
(quiet-run.9QbVfXJN). Final 55-scenario suites plus native preflight pass both Lisps:
evidence/notebook/{sbcl,ccl}-20261028-final/. Final regenerated inventory+13 checks,
255-entry zero-problem checker, isolated install/discovery/lookup, actual 182-export/
class/SETF coverage on both Lisps and 16 Python unit tests pass (quiet-run.BJXkcP6u).
Five package checks pass (evidence/packaging/notebook-20261028/,
quiet-run.dApyEmEn); package: dist/wxlambda-form-notebook. This packages the existing
form executable; the new notebook example/docs remain repository/docset deliverables.
Docset rebuilt after final session-note edits.

Physical tab/keyboard input, caption/native CHANGING behavior on hosted Windows/Linux,
graphical Cadre, a full multi-document editor and external clipboard interoperability
remain unverified. Next selected slice: single-selection list boxes, broadening the
tree/list-manager workflow without native container/document parity prerequisites.

## Single-selection list boxes (2026-10-28, verified)

ABI38 adds LIST-BOX, thin MAKE-LIST-BOX, ITEM-COUNT, generic ITEM-TEXT/SETF,
APPEND-ITEM, DELETE-ITEM and CLEAR-ITEMS. Existing SELECTION and ON/OFF-SELECTION
specialize on lists. Unsorted single selection only; initial finite proper string
list/index or NIL, direct/subclass CLOS/defaults/AFTER hooks and managed rollback.
Explicit UTF-8 byte lengths validate NUL and native arrays. Empty/duplicate labels
are valid. Counts: 189 exports, 133 named generics, 42 SETF generics, 16 window
wrapper classes (14 concrete+2 abstract), 56 GUI scenarios, 37 examples, 264 entries.

Append/rename preserve selection. Deleting the selected row clears it; deleting an
earlier row shifts the selected index down one. Clear resets all. Item mutations
are silent via temporary wxEventBlocker and restore managed handler-stack identity.
:NOTIFY T selection emits one synthetic native list-selection event after state
change, including unchanged/NIL assignments. Default writes are silent. No sorting,
multiselect, item client data, arbitrary styles/virtual models or stable native IDs.
Lisp domain models remain responsible for stable identity across item/index edits.

Two command kinds :LISTBOX-SELECTION/:LISTBOX-DOUBLE-CLICK carry cached string/index
payloads and raw action metadata. They support snapshots/contexts, contextual and
owned cloning, synthetic/owned dispatch including non-list handlers. Payload writes
do not change native items/selection. There are thirteen bindable built-ins, eleven
constructible built-ins and seven command kinds. Registered numbering starts after
both list types; ABI version protects mismatched libraries. No event record field
or immutable Lisp snapshot slot was added.

Independent review found a GTK activation post-callback native-widget dereference:
list-origin generic/propagated/convenience callbacks now protect list/ancestors from
destruction/close and reject modal/clipboard/native-loop entry until callback return.
Deleting-origin events suppress; non-list synthetic targets remain allowed. Item
edits and nested selection remain allowed. The native probe dispatches actual list
commands outside setters across all three callback paths, including callback
append/delete/clear and cloning. It passes with zero wx assertions, alongside both
focused Lisp suites (quiet-run.3HU3EgDp). Initial selection now also blocks native
selection events before publication. Additional standalone synthetic receiver and
handler-stack tests distinguish metadata behavior from control model changes.

examples/list-manager.lisp wires Append/Rename/Delete/Clear and selection/activation
feedback; bounded EXERCISE runs real native list operations. Context snapshots stay
unchanged after callback item edits, and handled callback errors preserve completed
selection while allowing recovery. Independent API review corrected choice-only
receiver/payload descriptions; shared ctrlsub inventory mappings were merged with
existing CHOICE selection mappings instead of duplicated. Inventory+13 checks and
264-entry zero-problem docset pass (quiet-run.Cr9JSlAg). Final native rebuild/harness
and all 56 GUI scenarios plus preflight pass both Lisps (quiet-run.CDYTWOPL):
evidence/list-box/{sbcl,ccl}-20261028-final/. Final inventory+13 checks, 264-entry
zero-problem checker, isolated install/discovery/lookup, actual 189-export/class/
SETF coverage on both Lisps and 16 Python unit checks pass (quiet-run.NmqyAM64).
Five package checks pass (evidence/packaging/list-box-20261028/,
quiet-run.FoJpsHAo); package: dist/wxlambda-form-list-box. This packages the existing
form executable; the new list-manager example/docs remain repository/docset
content. Final reference-field cleanup lists all thirteen native event types and
broadened selection receiver classes; docset rebuilt after those/session-note edits.

Physical mouse/keyboard activation, scrolling/accessibility, hosted Windows/Linux,
graphical Cadre, full tree/list-manager workflows and external clipboard interop
remain unverified. Next selected slice: bounded tree navigation and managed item
ownership; deeper list/data-view/virtual models stay demand-driven.

## Tree navigation and managed item views (2026-10-28, verified bounded scope)

ABI39 adds TREE-CONTROL, borrowed TREE-ITEM views, MAKE-TREE-CONTROL, root/child
creation, canonical root/parent/ordered child traversal, item text/expansion SETF,
item liveness, subtree deletion and EVENT-TREE-ITEM. Native-backed widget CLOS
construction/defaults/AFTER hooks/rollback remain; borrowed item construction rejects.
Counts: 202 exports, 144 named generics, 44 SETF generics, 17 window wrapper classes
(15 concrete+2 abstract) plus borrowed TREE-ITEM, 57 GUI scenarios, 38 examples,
279 docset entries. Event snapshots/FFI records append one checked tree-item token;
matching ABI39 library/FFI required. Native item pointers are never adopted/exposed.

Native registries track owner/parent/children with monotonic checked tokens; explicit
subtree invalidation, native delete hooks and early ManagedWindow destructor cleanup
invalidate IDs before reuse. A live-owner registry suppresses native-base teardown
callbacks. Lisp canonical views are session-rooted and revision-pruned after deletion/
normal boundaries/rollback, then cleared on cleanup. Failed Lisp publication deletes
the newly created node. Snapshot identity may outlive item validity/text: retain/copy
labels while live, and query liveness only within an active session.

Visible-root single selection only. AddRoot silently selects root; append preserves
selection. Nonempty selection writes require a same-tree item; NIL is empty no-op
only. Native notifying SelectItem is vetoable; silent default uses blockers. Collapse
promotes a selected descendant to the collapsed node. Delete preserves outside
selection or picks surviving parent/root; root deletion empties. No native clear/
hidden-root/multiselect, images/item data, label editing/drag/drop, virtual model or
all-tree-event depth. Item mutations from separate button callbacks are supported;
same-tree writes and modal/clipboard/pump entry defer during selection callbacks.

Independent review found missing session cache binding (fixed before item tests),
and generic tree UnselectAll/current/deferred-idle selection differences. The bounded
nonempty selection contract and synchronous valid fallback normalization eliminate
wrapper-induced delayed idle notifications. Tests check collapse/deletion after
PROCESS-EVENTS. The actual native wxTreeEvent + propagated parent probe checks item
metadata, protected lifetime/mutation/native-loop gates and clone/synthetic rejection.
Native raw arguments, cross-tree checks, native delete invalidation, non-reused bridge
tokens and raw native-base teardown pass with zero wx assertions (quiet-run.cIMIOvYM).
Focused SBCL+CCL tree/veto/ownership/idle/rollback tests pass (quiet-run.fLHGjdo3).
An initial direct ASDF load command lacked system registration; normal example/test
loading registers the local ASD before load. Final tests include immediate rollback
cache pruning and the example's item-text edit.

Two native notify kinds :TREE-SELECTION-CHANGING/:TREE-SELECTION-CHANGED carry managed
view metadata. Only CHANGING veto affects navigation. EVENT-SELECTION remains NIL
for tree kinds; old item/label events and owned/synthetic/clone support are unbound.
There are 15 bindable built-ins, 11 constructible built-ins and seven command kinds;
registered numbering starts after tree kinds. Inventory+13 checks and 279-entry
zero-problem docset pass (quiet-run.bKdLJbq8). All 57 GUI scenarios plus native
preflight pass both Lisps (quiet-run.bbsKvOUd):
evidence/tree-navigation/{sbcl,ccl}-20261028-final/. Inventory+13 checks,
279-entry zero-problem checker, isolated install/discovery/lookup, actual 202-export/
class/SETF coverage on both Lisps and 16 Python unit checks pass (quiet-run.zn1Gf46J).
The final port gate below was added afterward; host implementation behavior is
unchanged and post-gate focused/native/package checks pass separately. No whole file-manager or virtual/deep-tree completeness claim is made.

Physical tree mouse/keyboard/accessibility, large/deep/virtual trees, hosted Windows/
Linux GUI, graphical Cadre, allocation/IO-race injection and clipboard interoperability
remain unverified. Next selected slice: splitter windows for a two-pane tree/list
manager, rather than exhaustive tree event/data/native pointer depth.

Late port review found wxQt posts selection events asynchronously with raw item IDs.
The reviewed synchronous token/event translation cannot safely cover delayed stale
or recycled IDs, so wxQt TREE-CONTROL construction now rejects before allocation
until generation-safe async translation is implemented. Native Qt test branch checks
that explicit error when built there; Qt-hosted execution remains unrun. Current
macOS/generic/GTK/MSW implementation paths retain scope; hosted GTK/MSW still need
runtime verification. Post-gate main bridge/native harness, zero wx assertions,
focused tree tests on both Lisps and five package checks pass (quiet-run.aoqjFBv1).
Final package: dist/wxlambda-form-tree-navigation-final; evidence:
evidence/packaging/tree-navigation-20261028-final/. This packages the existing form
executable; new tree example/docs remain repository/docset deliverables. Final
inventory+13 checks and docset checker pass (quiet-run.jKBReUdM), and the docset is
rebuilt/installed after final session notes. Qt-hosted rejection is not claimed run.

## Fixed splitter panes (2026-10-28)

ABI40 adds native-backed SPLITTER-WINDOW and once-only attachment of two canonical
PANEL children, fixed orientation, bounded sash/minimum properties, gravity and
native sash notifications. See docs/SPLITTER.md and examples/splitter-manager.lisp.
Counts: 212 exports, 153 named generics, 48 SETF generics, 18 window wrapper classes
(16 concrete+2 abstract), borrowed TREE-ITEM, 58 GUI scenarios, 39 examples and
291 docset entries. Attachment is quiescent (outside RUN/pumps/callbacks); no
Unsplit/Replace/Initialize or broad splitter parity. Positive minimum and disabled
double-click keep panes attached. Candidate panes and native/propagated sash
callbacks protect lifetime, same-splitter properties and nested native loop entry.

Independent code/test review found no remaining actionable bugs after quiescent
attachment was enforced. Documentation review corrected factory generic metadata
and precise guard scope. Pinned generic native source confirms CHANGING edits/veto
require handling; CHANGED occurs after commit and cannot undo it. Native probe
exercises actual and propagated changes, -1/veto cancellation, observational
CHANGED, invalid ranges, double-click and teardown with zero wx assertions.
Focused native and SBCL/CCL tests/example pass (quiet-run.aQqhG6zS). Inventory
and 13 coverage checks pass (quiet-run.gHUfcUdl). Five isolated package checks
pass (quiet-run.a7fsFAuc): dist/wxlambda-form-splitter, evidence/packaging/splitter-20261028/.
The packaged executable remains the form; splitter example/docs are separate
repository/docset deliverables. Cadre installation follows docs/DOCSET.md.

Session handoff: broad GUI coverage with selective depth remains the objective.
Next selected slice: toolbar/status-bar actions for practical application shells.
Do not exhaust native splitter methods before proceeding. Physical sash dragging,
keyboard/accessibility, hosted Windows/Linux GUI and graphical Cadre remain
unverified. The tree-based demo retains the existing wxQt tree gate.

Final splitter verification: all 58 GUI scenarios pass on SBCL and CCL; evidence
at evidence/splitter/{sbcl,ccl}-20261028-final/. All 16 Python checks and actual
212-export/class/SETF docset coverage pass on both Lisps (quiet-run.PS1p1jl4);
291-entry checker, isolated installation, discovery, symbol lookup and guide/example
access pass after reviewed documentation fixes (quiet-run.yjJ6MnFp). The initial
Python run caught a stale expected result count; corrected to 59 including preflight.

## Frame toolbar/status actions (2026-10-28, bounded scope)

ABI41 adds TOOLBAR and STATUS-BAR window subclasses, thin generic factories,
fixed normal/check/separator toolbar tools addressed by toolbar-scoped positive
IDs, seven stock art choices, explicit once-only REALIZE-TOOLBAR, count and silent
generic SETF enabled/checked state, native accepted activation, fixed status field
count and generic SETF text. See docs/FRAME-ACTIONS.md and examples/frame-actions.lisp.
Counts: 225 exports, 164 named generics, 51 SETF generics, 20 window wrapper classes
(18 concrete+2 abstract) plus borrowed TREE-ITEM, 59 GUI scenarios, 40 examples,
306 docset entries. Frame chrome has no user sizers; direct bar deletion is
synchronous and quiescent. Frame ownership invalidates bars and failed CLOS
initialization cleans up native association/callback roots.

wxEVT_TOOL aliases wxEVT_MENU; reuse :MENU and action IDs. Native check state
toggles before action callbacks; programmatic activation returns accepted T,
not native handled status. Actual/native propagated toolbar-origin guards block
protected destruction/close, same-toolbar tool-state writes/activation, modal,
clipboard and nested dispatch/pump entry. Status feedback remains writable.
Status fields are application-owned: frame automatic help updates are disabled.
Example consumes toolbar events in a contextual handler and shares action logic
with menu events without duplicate propagation or implicit state synchronization.

Independent review found deferred native bar destruction and teardown callback
suppression issues; fixed with synchronous quiescent deletion and early owner
registry/pending-destruction guards. A native crash report showed Cocoa restoring
old raw tool-pointer placeholders in MacInstallNativeToolbar during toolbar
replacement at a reused C++ address. This was not treated as a passing retry.
OSX construction now explicitly selects wx's ordinary in-window toolbar backend,
restores the previous effective option string and rejects builds lacking system
options. Native Cocoa chrome remains unbound. wxSystemOptions cannot publicly
remove entries; absent-value restoration as empty can shadow future environment
changes, documented in the guide. Native probes test 24 realized/unrealized
replacements with prior empty/0/1 values, effective HasOption semantics and actual
OnLeftClick propagation outside bridge activation guards. Zero wx assertions,
focused tests and example pass on SBCL+CCL (quiet-run.eP4hjhBC). Review also fixed
the conditional native-feature probe and documentation SETF/guard/DPI wording.

Session handoff: continue broad GUI coverage with selective depth. Next selected
slice is keyboard accelerators for shared application actions. No dynamic tool
removal/reordering, radio/dropdown tools, custom images, status widths/stacks or
native Cocoa toolbar customization. Physical clicks/keyboard/accessibility, DPI
rendering, hosted Windows/Linux GUI and graphical Cadre remain unverified.

Final frame-actions verification: main/native builds, zero-assertion native probes
and all 59 GUI scenarios pass on both SBCL and CCL (quiet-run.OX3Jjd32); fresh
evidence: evidence/frame-actions/{sbcl,ccl}-20261028-final/. Regenerated inventory
and 13 coverage checks, 16 Python checks and 306-entry zero-problem docset pass
(quiet-run.bCtnmaJC). Actual 225-export/class/SETF lookup checks pass on both
Lisps, as do isolated install/discovery/symbol/guide/example access checks
(quiet-run.buyztgGM). Runtime counts match the handoff (quiet-run.tMekyyz3).
Five isolated package checks pass (quiet-run.OGWexn2d), package
dist/wxlambda-form-frame-actions, evidence/packaging/frame-actions-20261028/.
The package remains the existing form executable; the new frame-actions example
and guides are repository/docset deliverables, not bundled example launchers.
Documentation is rebuilt and checked after final evidence notes. Follow
docs/DOCSET.md to install for Cadre; graphical Cadre execution remains unrun.

## Keyboard accelerators for shared actions (2026-10-28, bounded scope)

ABI42 adds FRAME-ACCELERATORS and generic SETF with copied proper finite lists of
(KEY ID &rest MODIFIERS), plus FRAME :ACCELERATORS initarg. Keys: ASCII letters/
digits normalized uppercase, F1..F12; modifiers logical Control, Shift, Alt and
raw Control. Ordinary text keys require Control/Alt/raw-Control. Portable IDs
1..32767 avoid Windows WORD/signed-short command truncation. Max256 entries;
effective native duplicate chords reject (raw/control alias off OSX). Getter
returns fresh nested native-registry data and validates stale/empty frames.
No native entry wrappers, borrowed menu pointers, parsers or arbitrary key codes.
CMake explicitly requires wxUSE_ACCEL=1. Native entry/table construction and
quiescent whole-table replacement prepare data before publication. NIL clears
wxNullAcceleratorTable; failed CLOS initialization removes native tables/roots.
Counts: 226 exports, 165 named generics, 52 SETF generics, 20 window wrapper
classes (18 concrete+2 abstract) plus borrowed TREE-ITEM, 60 GUI scenarios,
41 examples and 309 docset entries.

Pinned-source review identified platform dispatch differences: OSX/GTK send direct
MENU commands with unset origin/default metadata, without menu enabled/check
semantics; MSW may resolve matching menu items, honor disabled state and toggle.
The new examples/keyboard-actions.lisp uses a shared Lisp model for menu/tool/key
actions, gates disabled actions and ignores native checked metadata. Contextual
handlers consume commands to avoid button fallback/duplicate action propagation.
It does not edit menu labels or own menu/system shortcuts.

Because no portable native accelerator origin marker exists, all managed MENU
callbacks conservatively protect all frames with nonempty explicit tables and
their descendants, including callbacks on app/pushed/unrelated handlers and
synthetic commands. Modal/clipboard/nested dispatch entry and protected subtree
destruction/close reject; status/model feedback remains writable. Clear removes
this extra policy; existing toolbar/navigation restrictions persist. Modal
Open/Save shortcuts are not complete: defer guarded work until callback return.
See docs/KEYBOARD-ACCELERATORS.md for exact scope and Cadre installation link.

Independent implementation/tests/documentation review found no remaining
actionable issues. Its source audit confirmed safe native table copying with
null menu pointers, OSX/GTK/MSW command differences and effective modifiers.
Focused example/table/rollback/guard tests pass on SBCL+CCL (quiet-run.rJy1SZav).
Native key tests enter child OSXHandleKeyEvent CHAR_HOOK directly (not just
synthetic MENU dispatch), traverse pushed handlers and frame tables, exercise
Control+A, F12 and Alt+0, and verify disabled/check-menu differences, clear/
modal/clipboard restoration, invalid atomic replacement and registry teardown
with zero wx assertions (quiet-run.t6ocsNyt). The first coverage validation
caught missing wxWindow accelerator-table records due class-owner parsing;
exact pinned Get/Set mappings were added and reviewed.

Session handoff: broad GUI coverage with selective depth remains the objective.
Next selected slice: deferred UI actions after native callbacks, enabling safe
modal/clipboard/destructive menu/tool/accelerator workflows before claiming
complete Open/Save shortcuts. No global hotkeys, key-event/text-input API,
arbitrary key/modifier/string parsing, menu shortcut ownership or deep native
accelerator parity. Physical typing/focus, accessibility/layout/input methods,
reserved-key conflicts, hosted Windows/Linux key translation and graphical
Cadre remain unverified. Synthetic example exercises check action logic only;
native CHAR_HOOK tests validate macOS dispatch, not physical keyboard input.

Final accelerator verification: all 60 GUI scenarios pass on SBCL and CCL
(quiet-run.ocLeRhGx), evidence/accelerators/{sbcl,ccl}-20261028-final/.
Regenerated inventory+13 coverage checks, all16 Python checks and309-entry
zero-problem docset pass; isolated install/discovery/symbol/guide/example access
and actual226-export/class/SETF coverage pass on both Lisps
(quiet-run.zgRvsN6b). Runtime counts verified (quiet-run.AsCvgs6k).
Five isolated package checks pass (quiet-run.2YUAvmvb), package
dist/wxlambda-form-accelerators, evidence/packaging/accelerators-20261028/.
This packages the existing form executable; the keyboard-actions example and
new guide remain repository/docset deliverables. Docset is rebuilt/checked and
installed to an isolated destination after these final evidence notes; see
docs/DOCSET.md for the standard Cadre installation path. Graphical Cadre unrun.

## Deferred UI actions (2026-10-28, bounded safe scheduling)

ABI43 adds opaque immutable UI-ACTION tickets/predicate, DEFER-UI-ACTION, generic
CANCEL-UI-ACTION and UI-ACTION-PENDING-P. Session roots retain zero-argument Lisp
functions; native FIFO entries contain only numeric tokens and optional checked
weak window owners. Max1024 pending entries. Scheduling never calls inline;
cancel removes queued work once, execution removes identity before invocation,
and active work cannot cancel/interrupt itself. Dead/pending owner or ancestor
suppresses delivery; roots retire when expiry is queried/reaped/delivered, not
synchronously in a native destructor. Session finish clears native work before
window/toolkit teardown and clears Lisp roots on exit.

Managed App::ProcessIdle calls the toolkit base first, then delivers at most one
action per safe idle iteration. RUN/full polling drive work; pending-only event
processing does not. Multiple iterations/actions can occur in one full poll.
Native-entry depth (thread-local), driver baseline, idle recursion, borrowed
context/callback, modal/dialog/clipboard, navigation/tool/accelerator, active
action, error and shutdown gates prevent early/reentrant delivery. wxWakeUpIdle
requests progress without busy requeueing during blocked modal loops. The
ProcessIdle override is present in native headers but omitted from pinned
interface declarations; wxApp and wxWakeUpIdle partial scope maps record the
internal prerequisite without inventing an inventory entry. No CallAfter/worker/
timer/public native virtual-hook parity is claimed.

A narrow action-phase exception permits structural operations inside RUN/poll:
menus, frame bars, table replacement, notebook/splitter attachment and managed
handler structures/pending-event removal. Normal nested event callback guards
remain; recursive RUN/pumps/finish reject. Modal/clipboard/destructive work is
safe in the outer action; current modal callbacks still use END-MODAL directly
since deferred actions wait until a modal exits. Action errors preserve the
original condition, pause later queued work and rethrow at outer RUN/poll.
Following work remains pending for recovery within the session. Nonlocal exits
are converted to contained errors before crossing the FFI boundary.

Example: examples/deferred-actions.lisp queues shared menu/tool/keyboard file
selection, text copy and confirmed close. It reports a selected path without
file IO; automatic EXERCISE injects chooser/clipboard/confirmation functions.
Separate tests use actual generic message/file dialogs with cancellation, test
sequential modal non-reentrancy, safe structural operations under RUN and polling,
FIFO, cancellation, owner/root expiry, stale contexts, error-pump/error-RUN,
nonlocal exit and shutdown. Native tests force idle in callbacks/synchronous
operations/modal loops, verify queue capacity, pending-error pause and cleanup,
and transfer UTF-8 through an isolated MemoryClipboard fixture. System clipboard
text remains untouched by automatic tests; live queries are availability only.

Independent implementation/documentation review found one threading defect:
central status() incremented a global native-entry counter before rejecting
wrong-thread calls. It is now thread-local; concurrent rejected worker probes
verify GUI counter isolation. DEFER validates native main-thread entry before
mutating Lisp roots. Enqueue wakes before publishing its record, so failed wake/
allocation cannot leave orphan work. Review wording fixes clarify modal actions
are allowed, owner pruning is observed, and structural SETF sections name the
safe-phase exception. No remaining actionable findings.

Counts: 231 exports, 167 named generics, 52 SETF generics, 20 window wrappers
(18 concrete+2 abstract), borrowed TREE-ITEM and immutable UI-ACTION ticket,
64 GUI scenarios (four deferred-action modes), 42 examples, 316 docset entries.
Focused normal both-Lisp tests pass (quiet-run.vVzXbIrn); initial native probes
and both-Lisp error-pump/error-RUN/nonlocal modes pass (quiet-run.5mzzVu9q).
Main/native rebuild, reviewed TLS/worker/isolated clipboard native probes,
regenerated inventory+14 coverage checks, 16 Python checks and zero-problem
316-entry docset pass (quiet-run.Tv4bdjcR).

Session handoff: broad GUI coverage with selective depth remains the goal.
Next selected slice: compose deferred Open/Save/Copy/Paste/close requests into
the persistent document form with toolbar and keyboard action entry points.
No background workers, timers/deadlines, promise/future or transactional rollback
API. Physical input, hosted Windows/Linux GUI/clipboard interoperability,
arbitrary allocation/IO-race injection and graphical Cadre remain unverified.

Final reviewed native build/probes and all64 GUI scenarios pass on SBCL+CCL
(quiet-run.zf8ajJSU); fresh evidence:
evidence/deferred-actions/{sbcl,ccl}-20261028-reviewed/. Both-Lisp actual231-export/
class/SETF coverage, runtime counts, 316-entry zero-problem docset, isolated
install/discovery/symbol lookup and guide/example access pass after documentation
review fixes (quiet-run.iqV75sWj). Five isolated package checks pass
(quiet-run.rEEbiJ7F); package dist/wxlambda-form-deferred-actions, evidence
evidence/packaging/deferred-actions-20261028/. This remains the existing packaged
form executable; the new deferred-action example/guide are separate repository/
docset deliverables. Rebuild/check/install docset after final evidence notes;
standard Cadre install instructions remain docs/DOCSET.md. Manual/hosted checks
listed above were not run and no physical clipboard/interoperability claim is made.

## Deferred document-form shell integration (2026-10-28)

The persistent form now composes existing ABI43 APIs for shared deferred Open,
Save, Save As, whole-name Copy/Paste and close actions across menus, a six-action
toolbar and configured keyboard shortcuts, with application-owned status-bar
feedback. IDs12101..12106 fit the portable accelerator limit; logical Control
shortcuts are O, S, Shift+S, W, Shift+C and Shift+V. Normal native text-selection
Copy/Paste shortcuts are not replaced. No public binding, native ABI or native
inventory status changes: 231 exports, 167 named generics, 52 SETF generics,
20 window wrappers plus borrowed TREE-ITEM/UI-ACTION ticket; 42 examples and
316 docset entries. GUI runner grows to65 scenarios plus preflight.

UI requests share one owner-bound pending ticket and a separate action-active
latch; existing busy protects synchronous command execution. First request wins
while queued or active, even across different commands. Views disable only during
the safe action body (same-toolbar callback writers remain guarded), then restore
if the frame survives. Document values/clipboard/dirty state are read at delivery,
not at click time. Canceled tickets recover without a stuck latch; disposal and
forced/non-vetoable close cancel queued work. Real document IO/validation and
clipboard persistence semantics are unchanged.

Vetoable native close is immediately vetoed and enqueues its decision. Cancel/
canceled save/failure leaves the form alive. Approval retries native close with
a scoped flag, consumed in the close handler and cleared in unwind cleanup.
A new integration test exposed a preceding handler's Veto+Skip reaching native
default destruction on replay; the form now consumes an already-observed veto
before the approved branch. The test proves blocked replay does not cache
authorization for a later dirty close. This does not promise arbitrary handler
ordering or that native close acceptance guarantees destruction. Initial
CLOSE-WINDOW returns NIL even for a form later closed by deferred replay.

Existing file/clipboard workflows pass on both Lisps after adaptation
(quiet-run.5aMwpD3U). New tests/document-form-actions.lisp verifies deferred
menu/tool/configured-shortcut command paths, exactly-one pending request,
coalescing, delivery-time values, cancel recovery, active-command rejection/
disabled views, read/write state, nil/empty/invalid clipboard input, reporter-error
cleanup, propagated veto/one-use close approval, forced/disposal cancellation
and delivery under RUN (quiet-run.ZD7F9ZM9). Clipboard fixtures do not access
system text; actual native default file/message adapters are canceled from idle
in tests/document-form.lisp. The example EXERCISE now requests toolbar Save and
pumps a scripted cancellation. Native key translation remains separately tested
by the accelerator library probes; synthetic form commands are not physical typing.

Independent implementation/tests/documentation review has no remaining findings.
All65 GUI scenarios pass on SBCL and CCL (quiet-run.ytZiIxlo), fresh evidence
evidence/document-form-actions/{sbcl,ccl}-20261028-final/. Regenerated unchanged
native inventory+14 checks, 16 Python checks and 316-entry zero-problem docset
pass (quiet-run.3c6G4u3H). This application composition does not implement or
check off native wxDocument/wxDocManager/stream or platform-native dialog APIs.

Session handoff: broad GUI coverage with selective depth remains the objective.
Next selected slice: bounded UI timers for periodic updates with owner/session
lifetime and cancellation, broadening monitoring apps rather than deepening
native document/editor parity. Physical menu/tool/keyboard/chooser interaction,
hosted Windows/Linux GUI/clipboard interoperability, graphical Cadre and arbitrary
allocation/IO-race injection remain unverified. The existing packaged form stays
the simple form app; integrated document-form sources/guides are repository/
docset deliverables, not a new binary launcher.

Final both-Lisp actual231-export/class/SETF coverage and316-entry zero-problem
docset, isolated install/discovery/symbol lookup/guide/example access pass
(quiet-run.UVOBKfYV). Five existing package isolation checks pass
(quiet-run.nHsjeRvG), package dist/wxlambda-form-document-actions, evidence
evidence/packaging/document-form-actions-20261028/. The package remains the
simple form executable as noted above. Rebuild/check/install docset after
final evidence notes; standard Cadre installation is documented in docs/DOCSET.md.
No native code changed, so native invariant probes from ABI43 were not repeated
for this application-only batch. Physical/hosted/manual checks listed above remain unrun.

## Bounded UI timers (2026-10-28, verified bounded scope)

ABI44 adds opaque immutable UI-TIMER tickets/predicate, START-UI-TIMER and generic
CANCEL-UI-TIMER/UI-TIMER-ACTIVE-P. Functions take no arguments. Supported interval
1..86400000ms, max256 live registrations; optional weak window owner or session
lifetime, periodic default/one-shot mode. No public native timer receiver, raw
adoption, restart/interval SETF or Notify override. Counts:236 exports,169 named
generics,52 SETF generics,20 window wrappers plus TREE-ITEM/UI-ACTION/UI-TIMER
identities,43 examples,69 GUI scenarios (four timer modes),323 docset entries.

Private wxTimer::Notify only marks one coalesced due tick/wakes idle; it never
calls Lisp or erases itself. Repeated native expirations do not create catch-up
counts. A shared sequence merges first-due timers with deferred action enqueue
order, one work item per safe idle iteration, separate from action queue capacity.
Existing native-entry/driver/idle/callback/modal/clipboard/operation/error/shutdown
gates remain, with an explicit Notify depth guard. Timer functions inherit the
safe structural phase, cannot recursively RUN/pump/finish and do not run while
modal/other safe work is active. Periodic functions can cancel themselves or
others without retaining a native pointer/iterator across Lisp. One-shot active
identity includes due-but-stopped native work, then retires before invocation.

Native timers run continuously; bridge one-shots stop at their first valid due
notification. An initial steady-clock lower bound ignores stale early native
notifications assigned to reused timer IDs (MSW), avoiding premature new one-shot
retirement without rearming inside Notify. No precision/cadence/deadline claim.
Observed owner/ancestor expiry stops/retires without user delivery, with native
object/root cleanup at query/reap/idle. Session finish clears timers before
window/toolkit teardown. Failed enqueue/start and managed owner initialization
rollback release roots. Escaping periodic function errors cancel that registration,
pause remaining work and retain original conditions at outer RUN/poll; one-shots
were already retired. Nonlocal exits remain contained by shared safe-work boundary.

Independent source review found that simple '3.2.12 or newer' OSX gating was
unsafe: audited official3.3.0/3.3.1 lack the queued-CF-timer validity check present
in the3.2.12 cancellation path. OSX timer creation now accepts exact verified
wxWidgets3.2.12 only; unverified versions reject before allocation. CMake requires
wxUSE_TIMER. Other backends are source-reviewed but hosted GUI unrun. Review
confirmed continuous-native/logical-one-shot policy, source lifetime, activity
semantics, coalescing/fair ordering and no native pointer across Lisp dispatch.
Documentation review has no remaining findings; activity wording explicitly
distinguishes periodic functions from one-shot retirement. Re-audit before
widening the macOS gate; do not infer safety from version numbers.

Example examples/periodic-status.lisp updates label/status text and Pause/Resume
cancels/creates registrations; EXERCISE uses real native timers, self-stopping
after two updates. Focused real periodic/one-shot/modal/self-cancel/RUN/owner/
rollback tests pass on SBCL+CCL (quiet-run.cognRp64). Native probes and error-pump/
error-RUN/nonlocal modes pass (quiet-run.1Rw64sLy). Final main/native builds,
zero-assertion native tests and all69 GUI scenarios pass on both Lisps
(quiet-run.S28GJs56), evidence/ui-timers/{sbcl,ccl}-20261028-final/. Native probes
independently force coalescing, fair action ordering, stale early first ticks,
due one-shot state, owner Notify retirement/cleanup, count bounds, worker
rejection and shutdown. Automatic tests do not read/replace system clipboard text.
Regenerated inventory+14 checks,16 Python checks and323-entry zero-problem docset
pass (quiet-run.sKED4wWk).

Session handoff: broad GUI coverage with selective depth remains the objective.
Next selected slice: bounded numeric input controls (slider/spin) for everyday
forms, rather than exhaustive timer/event/clock parity. Physical input, timer
precision/deadline metrics, hosted Windows/Linux GUI, OS clipboard interoperability,
allocation-race injection and graphical Cadre remain unverified.

Final both-Lisp actual236-export/class/SETF coverage and runtime counts,323-entry
zero-problem docset, isolated install/discovery/symbol lookup and guide/example
access pass after reviewed docs (quiet-run.JkFx8ffz). Five package isolation checks
pass (quiet-run.VBklIE0Q), package dist/wxlambda-form-ui-timers, evidence
evidence/packaging/ui-timers-20261028/. This remains the existing form binary;
periodic-status example and guide are repository/docset deliverables. Rebuild/
check/install docset after final evidence notes; standard Cadre installation
remains docs/DOCSET.md. No manual/hosted/timing-precision claims beyond the
explicit verification above.
