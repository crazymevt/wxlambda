# Implementation plan

Target a usable first release with the existing controls, reliable REPL-driven
development, validated platforms, and standalone packaging. Full wxWidgets
coverage is inventoried in Phase 6; its implementation remains future work.
This is a plan; unchecked steps are not claims of implemented or verified
behavior.

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

- [ ] Review exports, documentation, examples, ABI compatibility, and limitations.
- [ ] Run supported-platform and packaged-application checks.
- [ ] Align ASDF and CMake release versions.
- [ ] Prepare release notes and installation, development, debugging, and
  packaging instructions.

**Acceptance:** A new user can follow the documented installation-to-packaging
workflow.

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

Implement these as separate reviewable changes, beginning with baseline
corrections and REPL regression tests. Platform work can proceed alongside local
development when runners become available.
